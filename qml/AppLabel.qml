import QtQuick 2.12
import QtQuick.Controls 2.12

// Label with an explicit colour. The Controls 2 default follows the platform
// palette, and under Lomiri that is a light colour - which made every label
// without an explicit colour invisible against the app's light backgrounds.
Label {
    color: "#111111"
}
