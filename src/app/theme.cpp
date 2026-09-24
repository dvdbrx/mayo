/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "theme.h"

#include "../base/meta_enum.h"

#include <QtGui/QImage>
#include <QtGui/QPalette>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QSplitterHandle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QStyledItemDelegate>

#include <QtCore/QtDebug>

#include <unordered_map>

namespace Mayo {

namespace {

const QIcon& nullQIcon()
{
    static const QIcon null;
    return null;
}

// Provides a specific style dedicated to Mayo look and feel
// * One of the special things are the "header" combo boxes. This is the kind of QComboBox object
//   used in toolbar just below Mayo's main menubar.
//   These QComboBoxes look as "auto raised" QToolButtons and also the arrow indicator is more
//   visible
// * Height of the items in QComboBox popups are a bit enlarged
//   Qt5 needs a special ItemDelegate as QStyle::sizeFromContents() isn't used for combobox popups
class MayoStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle; // Inherit QProxyStyle constructors

    void setQComboBoxArrowPixmap(const QPixmap& pixmap)
    {
        m_qComboBoxArrowPixmap = pixmap;
    }

    void setQSplitterHighlightColor(const QColor& c)
    {
        m_qSplitterHighlightColor = c;
    }

    void drawControl(
            ControlElement elm, const QStyleOption* opt, QPainter* painter, const QWidget* widget
        ) const override
    {
        if (elm == QStyle::CE_Splitter)
            this->drawQSplitter(elm, opt, painter, widget);
        else
            QProxyStyle::drawControl(elm, opt, painter, widget);
    }

    void drawComplexControl(
            ComplexControl ctrl, const QStyleOptionComplex* opt, QPainter* painter, const QWidget* widget
        ) const override
    {
        if (ctrl == QStyle::CC_ComboBox && hasHeaderQComboBoxMark(widget))
            this->drawHeaderQComboBox(opt, painter, widget);
        else
            QProxyStyle::drawComplexControl(ctrl, opt, painter, widget);
    }

    QSize sizeFromContents(
            ContentsType type, const QStyleOption* opt, const QSize& size, const QWidget* widget
        ) const override
    {
        QSize sizeResult = QProxyStyle::sizeFromContents(type, opt, size, widget);
        if (type == CT_ItemViewItem) {
            auto comboBox = findQComboBoxParent(widget);
            if (comboBox)
                setComboBoxViewItemHeightHint(comboBox, &sizeResult);
        }

        return sizeResult;
    }

    void polish(QWidget* widget) override
    {
        QProxyStyle::polish(widget);
        if (qobject_cast<QSplitter*>(widget) || qobject_cast<QSplitterHandle*>(widget)) {
            widget->setMouseTracking(true);
            widget->setAttribute(Qt::WA_Hover, true);
        }

        if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            // Qt5/Fusion: sizeFromContents(CT_ItemViewItem) not used for popup item height
            // Use delegate approach instead
            comboBox->setItemDelegate(new ComboBoxItemDelegate);
#endif
        }
    }

    static void markAsHeaderQComboBox(QComboBox* cb)
    {
        cb->setProperty("mayo_isHeaderComboBox", true);
    }

    static bool hasHeaderQComboBoxMark(const QWidget* widget)
    {
        if (widget) {
            const QVariant v = widget->property("mayo_isHeaderComboBox");
            return !v.isNull() ? v.toBool() : false;
        }

        return false;
    }

    static void setComboBoxViewItemHeightHint(const QComboBox* cb, QSize* sizeHint)
    {
        if (cb && sizeHint) {
            const double f = hasHeaderQComboBoxMark(cb) ? 1.5 : 1.25;
            sizeHint->setHeight(sizeHint->height() * f);
        }
    }

private:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    class ComboBoxItemDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate; // Inherit QStyledItemDelegate constructors

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QSize size = QStyledItemDelegate::sizeHint(option, index);
            setComboBoxViewItemHeightHint(MayoStyle::findQComboBoxParent(option.widget), &size);
            return size;
        }
    };
#endif

    static QComboBox* findQComboBoxParent(const QWidget* widget)
    {
        QWidget* it = widget ? widget->parentWidget() : nullptr;
        while (it) {
            auto comboBox = qobject_cast<QComboBox*>(it);
            if (comboBox)
                return comboBox;
            else
                it = it->parentWidget();
        }

        return nullptr;
    }

    void drawQSplitter(
            ControlElement elm, const QStyleOption* opt, QPainter* painter, const QWidget* widget
        ) const
    {
        QProxyStyle::drawControl(elm, opt, painter, widget);
        if (opt && opt->state.testFlag(QStyle::State_MouseOver) && m_qSplitterHighlightColor.isValid())
            painter->fillRect(opt->rect, m_qSplitterHighlightColor);
    }

    void drawHeaderQComboBox(
            const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget
        ) const
    {
        if (
            option->state.testFlag(QStyle::State_Enabled)
            && option->state.testFlag(QStyle::State_Active)
            && option->state.testFlag(QStyle::State_MouseOver)
            )
        {
            QStyleOptionToolButton optsBtn;
            optsBtn.initFrom(widget);
            optsBtn.features = QStyleOptionToolButton::None;
            optsBtn.toolButtonStyle = Qt::ToolButtonIconOnly;
            if (option->state.testFlag(QStyle::State_On))
                optsBtn.state |= QStyle::State_On;

            this->drawPrimitive(QStyle::PE_PanelButtonTool, &optsBtn, painter, widget);
        }

        const QRect arrowRect = this->subControlRect(QStyle::CC_ComboBox, option, QStyle::SC_ComboBoxArrow, widget);
        const QRect arrowPixmapRect(
            arrowRect.left() + arrowRect.width() / 2 - (m_qComboBoxArrowPixmap.width() / 2),
            arrowRect.top() + arrowRect.height() / 2 - (m_qComboBoxArrowPixmap.height() / 2),
            m_qComboBoxArrowPixmap.width(),
            m_qComboBoxArrowPixmap.height()
        );
        painter->drawPixmap(arrowPixmapRect, m_qComboBoxArrowPixmap);
    }

    QPixmap m_qComboBoxArrowPixmap;
    QColor m_qSplitterHighlightColor;
};

QColor alphaChanged(const QColor& c, int alpha)
{
    QColor n = c;
    n.setAlpha(alpha);
    return n;
}

QPixmap invertedPixmap(const QPixmap& pix)
{
    QImage img = pix.toImage();
    img.invertPixels();
    return QPixmap::fromImage(img);
}

QString iconFileName(Theme::Icon icn)
{
    switch (icn) {
    case Theme::Icon::AddFile: return "add-file.svg";
    case Theme::Icon::File: return "file.svg";
    case Theme::Icon::OpenFiles: return "open-files.svg";
    case Theme::Icon::Import: return "import.svg";
    case Theme::Icon::Edit: return "edit.svg";
    case Theme::Icon::Export: return "export.svg";
    case Theme::Icon::Expand: return "expand.svg";
    case Theme::Icon::Cross: return "cross.svg";
    case Theme::Icon::Grid: return "grid.svg";
    case Theme::Icon::Link: return "link.svg";
    case Theme::Icon::Back: return "back.svg";
    case Theme::Icon::Next: return "next.svg";
    case Theme::Icon::Multiple: return "multiple.svg";
    case Theme::Icon::Camera: return "camera.svg";
    case Theme::Icon::LeftSidebar: return "left-sidebar.svg";
    case Theme::Icon::BackSquare: return "back-square.svg";
    case Theme::Icon::IndicatorDown: return "indicator-down_8.png";
    case Theme::Icon::Reload: return "reload.svg";
    case Theme::Icon::Stop: return "stop.svg";
    case Theme::Icon::Gear: return "gear.svg";
    case Theme::Icon::ZoomIn: return "zoom-in.svg";
    case Theme::Icon::ZoomOut: return "zoom-out.svg";
    case Theme::Icon::ClipPlane: return "clip-plane.svg";
    case Theme::Icon::Measure: return "measure.svg";
    case Theme::Icon::View3dIso: return "view-iso.svg";
    case Theme::Icon::View3dLeft: return "view-left.svg";
    case Theme::Icon::View3dRight: return "view-right.svg";
    case Theme::Icon::View3dTop: return "view-top.svg";
    case Theme::Icon::View3dBottom: return "view-bottom.svg";
    case Theme::Icon::View3dFront: return "view-front.svg";
    case Theme::Icon::View3dBack: return "view-back.svg";
    case Theme::Icon::VisibilityMenu: return "visibility-menu.svg";
    case Theme::Icon::VisibilityShowAll: return "visibility-show-all.svg";
    case Theme::Icon::VisibilityShowSelection: return "visibility-show-selection.svg";
    case Theme::Icon::VisibilityHideSelection: return "visibility-hide-selection.svg";
    case Theme::Icon::VisibilityShowSelectionOnly: return "visibility-show-selection-only.svg";
    case Theme::Icon::TurnClockwise: return "turn-cw.svg";
    case Theme::Icon::TurnCounterClockwise: return "turn-ccw.svg";
    case Theme::Icon::ItemMesh: return "item-mesh.svg";
    case Theme::Icon::ItemXde: return "item-xde.svg";
    case Theme::Icon::XdeAssembly: return "xde-assembly.svg";
    case Theme::Icon::XdeSimpleShape: return "xde-simple-shape.svg";
    }
    return {};
}

class ThemeClassic : public Theme {
public:
    QColor color(Color role) const override
    {
        const QPalette appPalette = qApp->palette();
        switch (role) {
        case Theme::Color::Palette_Base:
            return appPalette.color(QPalette::Base);
        case Theme::Color::Palette_Window:
            return appPalette.color(QPalette::Window);
        case Theme::Color::Palette_Button:
        case Theme::Color::ButtonFlat_Background:
            return appPalette.color(QPalette::Button);
        case Theme::Color::ButtonView3d_Background: {
            return appPalette.color(QPalette::Button);
        }
        case Theme::Color::ButtonFlat_Hover:
            return appPalette.color(QPalette::Button).darker(110);
        case Theme::Color::ButtonFlat_Checked:
            return appPalette.color(QPalette::Button).darker(125);
        case Theme::Color::ButtonView3d_Hover:
        case Theme::Color::ButtonView3d_Checked:
            return QColor{65, 200, 250};
        case Theme::Color::Graphic3d_AspectFillArea:
            return QColor{128, 200, 255};
        case Theme::Color::View3d_BackgroundGradientStart:
            return QColor{95, 120, 150};
        case Theme::Color::View3d_BackgroundGradientEnd:
            return QColor{218, 222, 228};
        case Theme::Color::RubberBandView3d_Line:
            return QColor{65, 200, 250};
        case Theme::Color::RubberBandView3d_Fill:
            return QColor{65, 200, 250}.lighter();
        case Theme::Color::MessageIndicator_InfoBackground:
            return QColor{128, 200, 255};
        case Theme::Color::MessageIndicator_InfoText:
        case Theme::Color::MessageIndicator_ErrorText:
            return appPalette.color(QPalette::WindowText);
        case Theme::Color::MessageIndicator_ErrorBackground:
            return QColor{225, 127, 127, 140};
        }
        return {};
    }

    const QIcon& icon(Icon icn) const override
    {
        auto it = m_mapIcon.find(icn);
        return it != m_mapIcon.cend() ? it->second : nullQIcon();
    }

    void setup() override
    {
        const QString icnBasePath = ":/images/themes/classic/";
        for (const Icon icn : MetaEnum::values<Theme::Icon>()) {
            const QString icnFileName = iconFileName(icn);
            m_mapIcon.emplace(icn, QIcon(QPixmap(icnBasePath + icnFileName)));
        }

        auto mayoStyle = new MayoStyle(qApp->style());
        mayoStyle->setQComboBoxArrowPixmap(QPixmap(":/images/themes/classic/indicator-down_8.png"));
        const QColor windowColor = qApp->palette().color(QPalette::Window);
        mayoStyle->setQSplitterHighlightColor(alphaChanged(windowColor.darker(120), 150));
        qApp->setStyle(mayoStyle);
    }

    void setupHeaderComboBox(QComboBox* cb) override
    {
        MayoStyle::markAsHeaderQComboBox(cb);
    }

private:
    std::unordered_map<Theme::Icon, QIcon> m_mapIcon;
};

class ThemeDark : public Theme {
public:
    QColor color(Color role) const override
    {
        const QPalette appPalette = qApp->palette();
        switch (role) {
        case Theme::Color::Palette_Base:
            return appPalette.color(QPalette::Base);
        case Theme::Color::Palette_Window:
            return appPalette.color(QPalette::Window);
        case Theme::Color::Palette_Button:
        case Theme::Color::ButtonFlat_Background:
            return appPalette.color(QPalette::Button);
        case Theme::Color::ButtonFlat_Hover:
            return QColor{34, 43, 60};
        case Theme::Color::ButtonFlat_Checked:
            return appPalette.color(QPalette::Highlight);
        case Theme::Color::ButtonView3d_Background:
            return QColor{18, 22, 30, 215};
        case Theme::Color::ButtonView3d_Hover:
            return QColor{34, 43, 60, 235};
        case Theme::Color::ButtonView3d_Checked:
            return appPalette.color(QPalette::Highlight);
        case Theme::Color::Graphic3d_AspectFillArea:
            return QColor{56, 189, 248};
        case Theme::Color::View3d_BackgroundGradientStart:
            return QColor{11, 14, 20};
        case Theme::Color::View3d_BackgroundGradientEnd:
            return QColor{22, 28, 40};
        case Theme::Color::RubberBandView3d_Line:
            return QColor{56, 189, 248};
        case Theme::Color::RubberBandView3d_Fill:
            return QColor{56, 189, 248, 45};
        case Theme::Color::MessageIndicator_InfoBackground:
            return QColor{37, 99, 235};
        case Theme::Color::MessageIndicator_InfoText:
            return Qt::white;
        case Theme::Color::MessageIndicator_ErrorText:
            return Qt::white;
        case Theme::Color::MessageIndicator_ErrorBackground:
            return QColor{225, 29, 72, 220};
        }
        return {};
    }

    const QIcon& icon(Icon icn) const override
    {
        auto it = m_mapIcon.find(icn);
        return it != m_mapIcon.cend() ? it->second : nullQIcon();
    }

    void setup() override
    {
        auto fnIsNeutralIcon = [](Icon icn) {
            return icn == Icon::AddFile || icn == Icon::OpenFiles || icn == Icon::Stop;
        };
        for (Icon icn : MetaEnum::values<Theme::Icon>()) {
            const QString icnFileName = iconFileName(icn);
            const QString icnBasePath =
                    !fnIsNeutralIcon(icn) ?
                        ":/images/themes/dark/" :
                        ":/images/themes/classic/"
                ;
            QPixmap pix(icnBasePath + icnFileName);
            if (!fnIsNeutralIcon(icn)) {
                const bool invertColors = icn != Icon::XdeAssembly && icn != Icon::XdeSimpleShape;
                if (invertColors)
                    pix = invertedPixmap(pix);
            }

            m_mapIcon.emplace(icn, pix);
        }

        QPalette p = qApp->palette();
        p.setColor(QPalette::Window, QColor{18, 21, 28});        // #12151c
        p.setColor(QPalette::Base, QColor{13, 16, 22});          // #0d1016
        p.setColor(QPalette::AlternateBase, QColor{18, 23, 32}); // #121720
        p.setColor(QPalette::Button, QColor{26, 32, 44});        // #1a202c
        p.setColor(QPalette::Mid, QColor{38, 48, 66});           // #263042
        p.setColor(QPalette::Dark, QColor{11, 14, 19});

        p.setColor(QPalette::Text, QColor{226, 232, 240});       // #e2e8f0
        p.setColor(QPalette::WindowText, QColor{241, 245, 249}); // #f1f5f9
        p.setColor(QPalette::ButtonText, QColor{241, 245, 249}); // #f1f5f9
        p.setColor(QPalette::PlaceholderText, QColor{100, 116, 139}); // #64748b

        p.setColor(QPalette::Highlight, QColor{37, 99, 235});    // #2563eb
        p.setColor(QPalette::HighlightedText, Qt::white);
        const QColor linkColor{56, 189, 248};                    // #38bdf8
        p.setColor(QPalette::Link, linkColor);
        p.setColor(QPalette::LinkVisited, linkColor);

        const QColor disabledBg{14, 17, 23};
        const QColor disabledText{71, 85, 105};                  // #475569
        p.setColor(QPalette::Disabled, QPalette::Window, disabledBg);
        p.setColor(QPalette::Disabled, QPalette::Base, disabledBg);
        p.setColor(QPalette::Disabled, QPalette::AlternateBase, disabledBg);
        p.setColor(QPalette::Disabled, QPalette::Button, disabledBg);
        p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
        p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
        qApp->setPalette(p);

        const QString css =
                R"(
                QMainWindow {
                    background-color: #12151c;
                }
                QWidget {
                    color: #e2e8f0;
                }
                QSplitter::handle {
                    background-color: #1d2330;
                }
                QSplitter::handle:hover {
                    background-color: #2563eb;
                }
                QSplitter::handle:horizontal {
                    width: 2px;
                }
                QSplitter::handle:vertical {
                    height: 2px;
                }
                QFrame[frameShape="4"], QFrame[frameShape="5"] {
                    color: #202634;
                    background-color: #202634;
                    border: none;
                    max-width: 1px;
                    max-height: 1px;
                }
                QMenuBar {
                    background-color: #12151c;
                    color: #e2e8f0;
                    border-bottom: 1px solid #1e2432;
                    padding: 2px 4px;
                }
                QMenuBar::item {
                    background: transparent;
                    padding: 5px 10px;
                    border-radius: 4px;
                    color: #e2e8f0;
                }
                QMenuBar::item:selected {
                    background-color: #1f2737;
                    color: #ffffff;
                }
                QMenuBar::item:pressed {
                    background-color: #2563eb;
                    color: #ffffff;
                }
                QMenu {
                    background-color: #141822;
                    border: 1px solid #283347;
                    border-radius: 6px;
                    padding: 4px;
                    color: #e2e8f0;
                }
                QMenu::item {
                    background: transparent;
                    padding: 6px 24px 6px 20px;
                    border-radius: 4px;
                    color: #e2e8f0;
                }
                QMenu::item:selected {
                    background-color: #2563eb;
                    color: #ffffff;
                }
                QMenu::item:disabled {
                    color: #526075;
                }
                QMenu::separator {
                    background-color: #222938;
                    height: 1px;
                    margin: 4px 6px;
                }
                QToolBar {
                    background-color: #12151c;
                    border-bottom: 1px solid #1e2432;
                    spacing: 3px;
                    padding: 3px 6px;
                }
                QToolBar::separator {
                    background-color: #222938;
                    width: 1px;
                    margin: 4px 6px;
                }
                QToolButton {
                    background-color: transparent;
                    border: 1px solid transparent;
                    border-radius: 4px;
                    padding: 4px;
                    color: #e2e8f0;
                }
                QToolButton:hover {
                    background-color: #1e2636;
                    border: 1px solid #2d394e;
                    color: #ffffff;
                }
                QToolButton:pressed {
                    background-color: #1d4ed8;
                    border: 1px solid #2563eb;
                    color: #ffffff;
                }
                QToolButton:checked {
                    background-color: #2563eb;
                    border: 1px solid #3b82f6;
                    color: #ffffff;
                }
                QToolButton:disabled {
                    color: #475569;
                }
                QPushButton {
                    background-color: #1a202c;
                    border: 1px solid #283347;
                    border-radius: 5px;
                    padding: 5px 14px;
                    color: #e2e8f0;
                    font-weight: 500;
                }
                QPushButton:hover {
                    background-color: #232b3c;
                    border-color: #3b4c68;
                    color: #ffffff;
                }
                QPushButton:pressed {
                    background-color: #1d4ed8;
                    border-color: #2563eb;
                    color: #ffffff;
                }
                QPushButton:disabled {
                    background-color: #12151c;
                    border-color: #1a202c;
                    color: #475569;
                }
                QComboBox {
                    background-color: #181d28;
                    border: 1px solid #283347;
                    border-radius: 5px;
                    padding: 4px 10px;
                    color: #e2e8f0;
                    combobox-popup: 0;
                }
                QComboBox:hover {
                    border-color: #3b4c68;
                    background-color: #1e2533;
                }
                QComboBox:on {
                    border-color: #2563eb;
                }
                QComboBox:disabled {
                    background-color: #12151c;
                    border-color: #1c222e;
                    color: #475569;
                }
                QComboBox::drop-down {
                    subcontrol-origin: padding;
                    subcontrol-position: top right;
                    width: 20px;
                    border-left: none;
                }
                QComboBox QAbstractItemView {
                    background-color: #141822;
                    border: 1px solid #283347;
                    border-radius: 6px;
                    selection-background-color: #2563eb;
                    selection-color: #ffffff;
                    padding: 4px;
                    outline: none;
                }
                QComboBox QAbstractItemView::item {
                    height: 24px;
                    padding: 2px 6px;
                    border-radius: 3px;
                    color: #e2e8f0;
                }
                QComboBox QAbstractItemView::item:hover {
                    background-color: #1f2737;
                    color: #ffffff;
                }
                QComboBox QAbstractItemView::item:selected {
                    background-color: #2563eb;
                    color: #ffffff;
                }
                QAbstractItemView {
                    show-decoration-selected: 1;
                    background-color: #0d1016;
                    alternate-background-color: #11151e;
                    color: #cbd5e1;
                    border: none;
                    outline: none;
                }
                QAbstractItemView::item {
                    height: 26px;
                    border-radius: 4px;
                    padding: 2px 4px;
                }
                QAbstractItemView::item:hover {
                    background-color: #1a2232;
                    color: #ffffff;
                }
                QAbstractItemView::item:selected {
                    background-color: #1d4ed8;
                    color: #ffffff;
                }
                QTreeView::branch {
                    background-color: transparent;
                }
                QHeaderView::section {
                    background-color: #131720;
                    color: #718096;
                    font-size: 11px;
                    font-weight: bold;
                    border: none;
                    border-bottom: 1px solid #1f2634;
                    padding: 4px 8px;
                }
                QLineEdit, QSpinBox, QDoubleSpinBox, QTextEdit {
                    background-color: #0d1016;
                    border: 1px solid #252e3f;
                    border-radius: 4px;
                    padding: 4px 8px;
                    color: #e2e8f0;
                    selection-background-color: #2563eb;
                    selection-color: #ffffff;
                }
                QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTextEdit:focus {
                    border: 1px solid #2563eb;
                }
                QScrollBar:vertical {
                    background-color: #0b0d13;
                    width: 9px;
                    margin: 0px;
                }
                QScrollBar::handle:vertical {
                    background-color: #232a39;
                    min-height: 20px;
                    border-radius: 4px;
                    margin: 2px;
                }
                QScrollBar::handle:vertical:hover {
                    background-color: #384660;
                }
                QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                    height: 0px;
                }
                QScrollBar:horizontal {
                    background-color: #0b0d13;
                    height: 9px;
                    margin: 0px;
                }
                QScrollBar::handle:horizontal {
                    background-color: #232a39;
                    min-width: 20px;
                    border-radius: 4px;
                    margin: 2px;
                }
                QScrollBar::handle:horizontal:hover {
                    background-color: #384660;
                }
                QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
                    height: 0px;
                }
                QTabWidget::pane {
                    border: 1px solid #1f2634;
                    background-color: #0d1016;
                }
                QTabBar::tab {
                    background-color: #131720;
                    color: #718096;
                    border: 1px solid #1f2634;
                    border-bottom: none;
                    padding: 6px 14px;
                    border-top-left-radius: 4px;
                    border-top-right-radius: 4px;
                    margin-right: 2px;
                }
                QTabBar::tab:selected {
                    background-color: #0d1016;
                    color: #38bdf8;
                    border-color: #1f2634;
                    border-bottom: 2px solid #38bdf8;
                    font-weight: bold;
                }
                QTabBar::tab:hover:!selected {
                    background-color: #181e2b;
                    color: #e2e8f0;
                }
                QStatusBar {
                    background-color: #0b0d12;
                    border-top: 1px solid #1a202c;
                    color: #718096;
                }
                QStatusBar QLabel {
                    color: #718096;
                }
                QGroupBox {
                    border: 1px solid #232b3b;
                    border-radius: 6px;
                    margin-top: 1.5ex;
                    padding-top: 8px;
                    font-weight: bold;
                    color: #8896ab;
                }
                QGroupBox::title {
                    subcontrol-origin: margin;
                    subcontrol-position: top left;
                    left: 8px;
                    padding: 0 4px;
                    background: transparent;
                }
                QProgressBar {
                    background-color: #0d1016;
                    border: 1px solid #232b3b;
                    border-radius: 4px;
                    text-align: center;
                    color: #e2e8f0;
                }
                QProgressBar::chunk {
                    background-color: #2563eb;
                    border-radius: 3px;
                }
                QFileDialog {
                    background-color: #12151c;
                }
                )";
        qApp->setStyleSheet(css);

        // Set style
        auto mayoStyle = new MayoStyle(QStyleFactory::create("Fusion"));
        mayoStyle->setQComboBoxArrowPixmap(QPixmap(":/images/themes/dark/indicator-down_8.png"));
        mayoStyle->setQSplitterHighlightColor(QColor{37, 99, 235, 180});
        qApp->setStyle(mayoStyle);
        qApp->setEffectEnabled(Qt::UI_AnimateCombo, false);
    }

    void setupHeaderComboBox(QComboBox* cb) override
    {
        MayoStyle::markAsHeaderQComboBox(cb);
    }

private:
    std::unordered_map<Theme::Icon, QIcon> m_mapIcon;
};

} // namespace

Theme* createTheme(const QString& key)
{
    if (key == "classic")
        return new ThemeClassic;

    if (key == "dark")
        return new ThemeDark;

    return nullptr;
}

} // namespace Mayo
