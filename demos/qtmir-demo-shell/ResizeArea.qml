import QtQuick 2.4
import QtMir.Application 0.1

MouseArea {
    id: root

    // to be set from outside
    property Item target
    property real borderThickness

    property bool leftBorder: false
    property bool rightBorder: false
    property bool topBorder: false
    property bool bottomBorder: false

    property bool dragging: false
    property real startX
    property real startY
    property real startWidth
    property real startHeight

    hoverEnabled: true

    property string cursorName: {
        if (containsMouse || pressed) {
            if (leftBorder && !topBorder && !bottomBorder) {
                return "left_side";
            } else if (rightBorder && !topBorder && !bottomBorder) {
                return "right_side";
            } else if (topBorder && !leftBorder && !rightBorder) {
                return "top_side";
            } else if (bottomBorder && !leftBorder && !rightBorder) {
                return "bottom_side";
            } else if (leftBorder && topBorder) {
                return "top_left_corner";
            } else if (leftBorder && bottomBorder) {
                return "bottom_left_corner";
            } else if (rightBorder && topBorder) {
                return "top_right_corner";
            } else if (rightBorder && bottomBorder) {
                return "bottom_right_corner";
            } else {
                return "";
            }
        } else {
            return "";
        }
    }
    onCursorNameChanged: {
        Mir.cursorName = cursorName;
    }

    function updateBorders() {
        leftBorder = mouseX <= borderThickness;
        rightBorder = mouseX >= width - borderThickness;
        topBorder = mouseY <= borderThickness;
        bottomBorder = mouseY >= height - borderThickness;
    }

    onPressedChanged: {
        if (pressed) {
            var pos = mapToItem(target.parent, mouseX, mouseY);
            startX = pos.x;
            startY = pos.y;
            startWidth = target.width;
            startHeight = target.height;
            dragging = true;
        } else {
            dragging = false;
            if (containsMouse) {
                updateBorders();
            }
        }
    }

    onEntered: {
        if (!pressed) {
            updateBorders();
        }
    }

    onPositionChanged: {
        if (!pressed) {
            updateBorders();
        }

        if (!dragging) {
            return;
        }

        var pos = mapToItem(target.parent, mouse.x, mouse.y);

        if (leftBorder) {
            if (startX + startWidth - pos.x > target.minWidth) {
                target.x = pos.x;
                target.width = startX + startWidth - target.x;
                startX = target.x;
                startWidth = target.width;
            }

        } else if (rightBorder) {
            var deltaX = pos.x - startX;
            if (startWidth + deltaX >= target.minWidth) {
                target.width = startWidth + deltaX;
            } else {
                target.width = target.minWidth;
            }
        }

        if (topBorder) {
            if (startY + startHeight - pos.y > target.minHeight) {
                target.y = pos.y;
                target.height = startY + startHeight - target.y;
                startY = target.y;
                startHeight = target.height;
            }

        } else if (bottomBorder) {
            var deltaY = pos.y - startY;
            if (startHeight + deltaY >= target.minHeight) {
                target.height = startHeight + deltaY;
            } else {
                target.height = target.minHeight;
            }
        }
    }
}

