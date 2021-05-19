/*
 * Copyright (C) 2013-2015 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "debughelpers.h"
#include <QTouchEvent>

#include <miral/toolkit_event.h>

// Unity API
#include <lomiri/shell/application/ApplicationInfoInterface.h>
#include <lomiri/shell/application/Mir.h>

const char *touchPointStateToString(Qt::TouchPointState state)
{
    switch (state) {
    case Qt::TouchPointPressed:
        return "pressed";
    case Qt::TouchPointMoved:
        return "moved";
    case Qt::TouchPointStationary:
        return "stationary";
    case Qt::TouchPointReleased:
        return "released";
    default:
        return "UNKNOWN!";
    }
}

QString touchEventToString(const QTouchEvent *ev)
{
    QString message;

    switch (ev->type()) {
    case QEvent::TouchBegin:
        message.append("TouchBegin ");
        break;
    case QEvent::TouchUpdate:
        message.append("TouchUpdate ");
        break;
    case QEvent::TouchEnd:
        message.append("TouchEnd ");
        break;
    case QEvent::TouchCancel:
        message.append("TouchCancel ");
        break;
    default:
        message.append("TouchUNKNOWN ");
    }

    for (int i=0; i < ev->touchPoints().size(); ++i) {

        const QTouchEvent::TouchPoint& touchPoint = ev->touchPoints().at(i);
        message.append(
            QStringLiteral("(id:%1, state:%2, scenePos:(%3,%4), pos:(%5,%6)) ")
                .arg(touchPoint.id())
                .arg(touchPointStateToString(touchPoint.state()))
                .arg(touchPoint.scenePos().x())
                .arg(touchPoint.scenePos().y())
                .arg(touchPoint.pos().x())
                .arg(touchPoint.pos().y())
            );
    }

    return message;
}

QString mirSurfaceAttribAndValueToString(MirWindowAttrib attrib, int value)
{
    QString str;

    switch (attrib) {
    case mir_window_attrib_type:
        str = QStringLiteral("type=%1").arg(mirSurfaceTypeToStr(value));
        break;

    case mir_window_attrib_state:
        str = QStringLiteral("state=%1").arg(mirSurfaceStateToStr(value));
        break;

    case mir_window_attrib_swapinterval:
        str = QStringLiteral("swapinterval=%1").arg(value);
        break;

    case mir_window_attrib_focus:
        str = QStringLiteral("focus=%1").arg(mirSurfaceFocusStateToStr(value));
        break;

    case mir_window_attrib_dpi:
        str = QStringLiteral("dpi=%1").arg(value);
        break;

    case mir_window_attrib_visibility:
        str = QStringLiteral("visibility=%1").arg(mirSurfaceVisibilityToStr(value));
        break;
    default:
        str = QStringLiteral("type'%1'=%2").arg((int)attrib).arg(value);
    }

    return str;
}

const char *mirSurfaceTypeToStr(int value)
{
    switch (value) {
    case mir_window_type_normal:       return "normal";        /**< AKA "regular"                   */
    case mir_window_type_utility:      return "utility";       /**< AKA "floating regular"          */
    case mir_window_type_dialog:       return "dialog";
    case mir_window_type_gloss:        return "gloss";
    case mir_window_type_freestyle:    return "freestyle";
    case mir_window_type_menu:         return "menu";
    case mir_window_type_inputmethod:  return "input Method";  /**< AKA "OSK" or handwriting etc.   */
    case mir_window_type_satellite:    return "satellite";     /**< AKA "toolbox"/"toolbar"         */
    case mir_window_type_tip:          return "tip";           /**< AKA "tooltip"                   */
    case mir_window_types:             Q_UNREACHABLE();
    }
    Q_UNREACHABLE();
}

const char *mirSurfaceStateToStr(int value)
{
    switch (value) {
    case mir_window_state_unknown:
        return "unknown";
    case mir_window_state_restored:
        return "restored";
    case mir_window_state_minimized:
        return "minimized";
    case mir_window_state_maximized:
        return "maximized";
    case mir_window_state_vertmaximized:
        return "vertmaximized";
    case mir_window_state_horizmaximized:
        return "horizmaximized";
    case mir_window_state_fullscreen:
        return "fullscreen";
    case mir_window_state_hidden:
        return "hidden";
    default:
        return "???";
    }
}

const char *mirSurfaceFocusStateToStr(int value)
{
    switch (value) {
    case mir_window_focus_state_unfocused:
        return "unfocused";
    case mir_window_focus_state_focused:
        return "focused";
    default:
        return "???";
    }
}

const char *mirSurfaceVisibilityToStr(int value)
{
    switch (value) {
    case mir_window_visibility_occluded:
        return "occluded";
    case mir_window_visibility_exposed:
        return "exposed";
    default:
        return "???";
    }
}

using namespace lomiri::shell::application;

const char *applicationStateToStr(int state)
{
    switch (state) {
    case ApplicationInfoInterface::Starting:
        return "starting";
    case ApplicationInfoInterface::Running:
        return "running";
    case ApplicationInfoInterface::Suspended:
        return "suspended";
    case ApplicationInfoInterface::Stopped:
        return "stopped";
    default:
        return "???";
    }
}

QString mirPointerEventToString(MirPointerEvent const* event)
{
    QString string = QStringLiteral("MirPointerEvent(x=%1,y=%2,relative_x=%3,relative_y=%4)")
        .arg(miral::toolkit::mir_pointer_event_axis_value(event, mir_pointer_axis_x))
        .arg(miral::toolkit::mir_pointer_event_axis_value(event, mir_pointer_axis_y))
        .arg(miral::toolkit::mir_pointer_event_axis_value(event, mir_pointer_axis_relative_x))
        .arg(miral::toolkit::mir_pointer_event_axis_value(event, mir_pointer_axis_relative_y));

    return string;
}

QString mirTouchEventToString(MirTouchEvent const* event)
{
    const int pointerCount = miral::toolkit::mir_touch_event_point_count(event);

    QString string(QStringLiteral("MirTouchEvent("));

    for (int i = 0; i < pointerCount; ++i) {

        if (i > 0) {
            string.append(",");
        }

        MirTouchAction touchAction = miral::toolkit::mir_touch_event_action(event, i);

        QString touchStr = QStringLiteral("(id=%1,action=%2,x=%3,y=%4)")
            .arg(miral::toolkit::mir_touch_event_id(event, i))
            .arg(mirTouchActionToString(touchAction))
            .arg(miral::toolkit::mir_touch_event_axis_value(event, i, mir_touch_axis_x))
            .arg(miral::toolkit::mir_touch_event_axis_value(event, i, mir_touch_axis_y));

        string.append(touchStr);
    }

    string.append(")");

    return string;
}

const char *mirTouchActionToString(MirTouchAction touchAction)
{
    switch (touchAction)
    {
    case mir_touch_action_up:
        return "up";
        break;
    case mir_touch_action_down:
        return "down";
        break;
    case mir_touch_action_change:
        return "change";
        break;
    default:
        return "???";
        break;
    }
}

namespace {
const char *mirKeyboardActionToString(MirKeyboardAction keyboardAction)
{
    switch (keyboardAction)
    {
    case mir_keyboard_action_up:
        return "up";
    case mir_keyboard_action_down:
        return "down";
    case mir_keyboard_action_repeat:
        return "repeat";
    default:
        return "???";
        break;
    }
}

QString mirInputEventModifiersToString(MirInputEventModifiers modifiers)
{
    QString string;

    if (modifiers != mir_input_event_modifier_none) {
        #define PRINT_MODIFIER(NAME) \
        if (modifiers & mir_input_event_modifier_##NAME) { \
            if (string.size() > 0) { string.append(","); } \
            string.append(#NAME); \
        }
        PRINT_MODIFIER(alt)
        PRINT_MODIFIER(alt_left)
        PRINT_MODIFIER(alt_right)
        PRINT_MODIFIER(shift)
        PRINT_MODIFIER(shift_left)
        PRINT_MODIFIER(shift_right)
        PRINT_MODIFIER(sym)
        PRINT_MODIFIER(function)
        PRINT_MODIFIER(ctrl)
        PRINT_MODIFIER(ctrl_left)
        PRINT_MODIFIER(ctrl_right)
        PRINT_MODIFIER(meta)
        PRINT_MODIFIER(meta_left)
        PRINT_MODIFIER(meta_right)
        PRINT_MODIFIER(caps_lock)
        PRINT_MODIFIER(num_lock)
        PRINT_MODIFIER(scroll_lock)
        #undef PRINT_MODIFIER
    }

    return string;
}
} // anonymous namespace

QString mirKeyboardEventToString(MirKeyboardEvent const* event)
{
    MirKeyboardAction keyboardAction = miral::toolkit::mir_keyboard_event_action(event);

    xkb_keysym_t keyCode = miral::toolkit::mir_keyboard_event_key_code(event);

    MirInputEventModifiers modifiers = miral::toolkit::mir_keyboard_event_modifiers(event);

    return QStringLiteral("MirKeyboardEvent(action=%1,key_code=0x%2,modifiers=[%3])")
        .arg(mirKeyboardActionToString(keyboardAction))
        .arg(keyCode, 4, 16, QLatin1Char('0'))
        .arg(mirInputEventModifiersToString(modifiers));
}

const char *qtCursorShapeToStr(Qt::CursorShape shape)
{
    switch(shape) {
    case Qt::ArrowCursor:
        return "Arrow";
    case Qt::UpArrowCursor:
        return "UpArrow";
    case Qt::CrossCursor:
        return "Cross";
    case Qt::WaitCursor:
        return "Wait";
    case Qt::IBeamCursor:
        return "IBeam";
    case Qt::SizeVerCursor:
        return "SizeVer";
    case Qt::SizeHorCursor:
        return "SizeHor";
    case Qt::SizeBDiagCursor:
        return "SizeBDiag";
    case Qt::SizeFDiagCursor:
        return "SizeFDiag";
    case Qt::SizeAllCursor:
        return "SizeAll";
    case Qt::BlankCursor:
        return "Blank";
    case Qt::SplitVCursor:
        return "SplitV";
    case Qt::SplitHCursor:
        return "SplitH";
    case Qt::PointingHandCursor:
        return "PointingHand";
    case Qt::ForbiddenCursor:
        return "Forbidden";
    case Qt::WhatsThisCursor:
        return "WhatsThis";
    case Qt::BusyCursor:
        return "Busy";
    case Qt::OpenHandCursor:
        return "OpenHand";
    case Qt::ClosedHandCursor:
        return "ClosedHand";
    case Qt::DragCopyCursor:
        return "DragCopy";
    case Qt::DragMoveCursor:
        return "DragMove";
    case Qt::DragLinkCursor:
        return "DragLink";
    case Qt::BitmapCursor:
        return "Bitmap";
    default:
        return "???";
    }
}

const char *unityapiMirStateToStr(int state)
{
    switch (state) {
    case Mir::UnknownState:
        return "unknown";
    case Mir::RestoredState:
        return "restored";
    case Mir::MinimizedState:
        return "minimized";
    case Mir::MaximizedState:
        return "maximized";
    case Mir::VertMaximizedState:
        return "vertMaximized";
    case Mir::FullscreenState:
        return "fullscreen";
    case Mir::HorizMaximizedState:
        return "horizMaximized";
    case Mir::MaximizedLeftState:
        return "maximizedLeft";
    case Mir::MaximizedRightState:
        return "maximizedRight";
    case Mir::MaximizedTopLeftState:
        return "maximizedTopLeft";
    case Mir::MaximizedTopRightState:
        return "maximizedTopRight";
    case Mir::MaximizedBottomLeftState:
        return "maximizedBottomLeft";
    case Mir::MaximizedBottomRightState:
        return "maximizedBottomRight";
    case Mir::HiddenState:
        return "hidden";
    default:
        return "???";
    }
}
