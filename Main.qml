import QtQuick
import QtQuick.Layouts
import Md3

Md3ApplicationWindow {
    id: window
    width: 640
    height: 360
    minimumWidth: 560
    minimumHeight: 320
    maximumWidth: 720
    maximumHeight: 420
    visible: true
    title: qsTr("新建 Md3 项目")
    color: Md3Theme.colorScheme.surface
    roundedCorners: true
    cornerRadius: Md3WindowCapabilities.windowCornerRadius

    Component.onCompleted: {
        Md3Theme.applySeed("#6750A4")
        selectPreferredKits()
    }

    property int step: 0
    readonly property int stepCount: 4
    property var selectedKitIndexes: []
    property int templateIndex: 1
    property bool md3Absolute: false
    property bool copyLibrary: true
    property bool autoDedupeName: true
    property string resolvedName: "MyMd3App"
    readonly property var templateIds: ["empty", "basic", "rail"]
    readonly property var stepTitles: [
        qsTr("项目"),
        qsTr("模板"),
        qsTr("编译器"),
        qsTr("库与创建")
    ]
    readonly property var stepHints: [
        qsTr("名称与输出路径"),
        qsTr("应用骨架"),
        qsTr("可多选 Kit"),
        qsTr("自定义库目录")
    ]

    function selectPreferredKits() {
        const kits = ProjectGenerator.kits
        if (kits.length === 0) {
            selectedKitIndexes = []
            return
        }
        let idx = -1
        for (let i = 0; i < kits.length; ++i) {
            const k = kits[i]
            if (String(k.version).indexOf("6.10") === 0 && String(k.kit).indexOf("mingw") >= 0) {
                idx = i
                break
            }
        }
        if (idx < 0)
            idx = kits.length - 1
        selectedKitIndexes = [idx]
    }

    function isKitSelected(index) {
        return selectedKitIndexes.indexOf(index) >= 0
    }

    function toggleKit(index) {
        const copy = selectedKitIndexes.slice()
        const pos = copy.indexOf(index)
        if (pos >= 0)
            copy.splice(pos, 1)
        else
            copy.push(index)
        copy.sort(function (a, b) { return a - b })
        selectedKitIndexes = copy
    }

    function selectedKits() {
        const kits = ProjectGenerator.kits
        const out = []
        for (let i = 0; i < selectedKitIndexes.length; ++i) {
            const idx = selectedKitIndexes[i]
            if (idx >= 0 && idx < kits.length)
                out.push(kits[idx])
        }
        return out
    }

    function selectedKitsLabel() {
        const ks = selectedKits()
        if (ks.length === 0)
            return "—"
        if (ks.length === 1)
            return ks[0].label
        return qsTr("%1 个 Kit").arg(ks.length)
    }

    function refreshResolvedName() {
        const desired = nameField ? nameField.text.trim() : "MyMd3App"
        const out = outField ? outField.text.trim() : ""
        const overwrite = forceSwitch && forceSwitch.checked
        if (!autoDedupeName || overwrite) {
            resolvedName = desired.length ? desired : "MyMd3App"
            return
        }
        resolvedName = ProjectGenerator.suggestProjectName(out, desired.length ? desired : "MyMd3App")
    }

    function canNext() {
        switch (step) {
        case 0:
            return nameField.text.trim().length > 0 && outField.text.trim().length > 0
        case 1:
            return true
        case 2:
            return selectedKitIndexes.length > 0
        case 3:
            return ProjectGenerator.isValidMd3Path(md3Field.text.trim())
        }
        return false
    }

    function goNext() {
        if (step < stepCount - 1) {
            if (step === 0)
                refreshResolvedName()
            step++
        } else {
            doGenerate()
        }
    }

    function goBack() {
        if (step > 0)
            step--
    }

    function doGenerate() {
        refreshResolvedName()
        const ok = ProjectGenerator.generate({
            name: nameField.text.trim(),
            outputDir: outField.text.trim(),
            uri: uriField.text.trim() || nameField.text.trim(),
            template: templateIds[templateIndex],
            kits: selectedKits(),
            md3Path: md3Field.text.trim(),
            md3Absolute: copyLibrary ? false : md3Absolute,
            copyLibrary: copyLibrary,
            vendorFolder: "vendor/Md3",
            dark: darkSwitch.checked,
            seed: seedField.text.trim() || "#6750A4",
            force: forceSwitch.checked
        })
        if (ok) {
            snack.show(qsTr("已生成 ") + ProjectGenerator.lastOutput.split(/[/\\]/).pop())
            step = stepCount
        } else {
            snack.show(ProjectGenerator.lastError || qsTr("生成失败"))
        }
    }

    titleBar: Component {
        Md3TitleBar {
            title: window.title
            showThemeToggle: true
            preferredHeight: 28
            barHeight: 28
        }
    }

    Item {
        anchors.fill: parent

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // Left: steps
            Rectangle {
                Layout.preferredWidth: 148
                Layout.fillHeight: true
                radius: Md3Theme.shape.medium
                color: Md3Theme.colorScheme.surfaceContainerLow

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4

                    Text {
                        text: qsTr("步骤")
                        color: Md3Theme.colorScheme.colorOnSurfaceVariant
                        font.family: Md3Theme.typography.fontFamily
                        font.pixelSize: 11
                    }

                    Repeater {
                        model: window.stepCount
                        delegate: Item {
                            required property int index
                            width: parent.width
                            height: 44
                            opacity: window.step >= window.stepCount ? 0.55 : 1

                            Rectangle {
                                anchors.fill: parent
                                radius: Md3Theme.shape.small
                                color: index === window.step && window.step < window.stepCount
                                       ? Md3Theme.colorScheme.secondaryContainer
                                       : "transparent"
                            }

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8

                                Rectangle {
                                    width: 22
                                    height: 22
                                    radius: 11
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: {
                                        if (index < window.step || window.step >= window.stepCount)
                                            return Md3Theme.colorScheme.primary
                                        if (index === window.step)
                                            return Md3Theme.colorScheme.primary
                                        return Md3Theme.colorScheme.surfaceContainerHighest
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: index < window.step || window.step >= window.stepCount
                                              ? "✓" : String(index + 1)
                                        color: (index <= window.step || window.step >= window.stepCount)
                                               ? Md3Theme.colorScheme.colorOnPrimary
                                               : Md3Theme.colorScheme.colorOnSurfaceVariant
                                        font.pixelSize: 11
                                        font.family: Md3Theme.typography.fontFamily
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 0
                                    Text {
                                        text: window.stepTitles[index]
                                        color: Md3Theme.colorScheme.colorOnSurface
                                        font.family: Md3Theme.typography.fontFamily
                                        font.pixelSize: 13
                                        font.weight: index === window.step ? Font.Medium : Font.Normal
                                    }
                                    Text {
                                        text: window.stepHints[index]
                                        color: Md3Theme.colorScheme.colorOnSurfaceVariant
                                        font.family: Md3Theme.typography.fontFamily
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: window.step < window.stepCount && index <= window.step
                                onClicked: window.step = index
                            }
                        }
                    }
                }
            }

            // Right: content + actions
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                Text {
                    text: window.step >= window.stepCount
                          ? qsTr("完成")
                          : window.stepTitles[window.step]
                    color: Md3Theme.colorScheme.colorOnSurface
                    font.family: Md3Theme.typography.fontFamily
                    font.pixelSize: Md3Theme.typography.titleMedium.size
                    font.weight: Font.Medium
                }

                Md3LinearProgressIndicator {
                    Layout.fillWidth: true
                    value: window.step >= window.stepCount ? 1 : (window.step + 1) / window.stepCount
                }

                StackLayout {
                    id: stack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: Math.min(window.step, window.stepCount - 1)
                    visible: window.step < window.stepCount

                    // 0 project — horizontal fields
                    ColumnLayout {
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Md3TextField {
                                id: nameField
                                Layout.fillWidth: true
                                label: qsTr("项目名称")
                                text: "MyMd3App"
                                onTextChanged: window.refreshResolvedName()
                            }
                            Md3TextField {
                                id: uriField
                                Layout.fillWidth: true
                                label: qsTr("QML URI")
                                placeholderText: qsTr("可空=项目名")
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Md3TextField {
                                id: outField
                                Layout.fillWidth: true
                                label: qsTr("输出目录")
                                text: {
                                    const md3 = ProjectGenerator.md3Path
                                    const i = Math.max(md3.lastIndexOf("/"), md3.lastIndexOf("\\"))
                                    return i > 0 ? md3.substring(0, i) : md3
                                }
                                onTextChanged: window.refreshResolvedName()
                                Component.onCompleted: window.refreshResolvedName()
                            }
                            Md3IconButton {
                                icon: "folder_open"
                                onClicked: {
                                    const d = ProjectGenerator.pickDirectory(qsTr("输出目录"), outField.text)
                                    if (d) {
                                        outField.text = d
                                        // Use folder name as project name if still default
                                        const base = d.replace(/\\/g, "/").split("/").pop()
                                        if (base && /^[A-Za-z][A-Za-z0-9_]*$/.test(base)
                                                && (nameField.text === "MyMd3App" || nameField.text.length === 0))
                                            nameField.text = base
                                        window.refreshResolvedName()
                                    }
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: {
                                const conflict = ProjectGenerator.projectExists(outField.text, nameField.text.trim())
                                const overwrite = forceSwitch && forceSwitch.checked
                                if (overwrite)
                                    return qsTr("将写入（覆盖）：%1/%2").arg(outField.text).arg(nameField.text.trim())
                                if (conflict && window.resolvedName !== nameField.text.trim())
                                    return qsTr("名称已占用 → 自动使用：%1/%2")
                                          .arg(outField.text).arg(window.resolvedName)
                                return qsTr("创建为：%1/%2").arg(outField.text).arg(window.resolvedName)
                            }
                            color: Md3Theme.colorScheme.colorOnSurfaceVariant
                            font.family: Md3Theme.typography.fontFamily
                            font.pixelSize: 11
                        }
                        Item { Layout.fillHeight: true }
                    }

                    // 1 template — horizontal cards
                    RowLayout {
                        spacing: 8
                        Repeater {
                            model: [
                                { title: qsTr("空白"), sub: qsTr("空窗口") },
                                { title: qsTr("基础"), sub: qsTr("按钮示例") },
                                { title: qsTr("导航"), sub: qsTr("Rail 多页") }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: Md3Theme.shape.medium
                                color: index === window.templateIndex
                                       ? Md3Theme.colorScheme.secondaryContainer
                                       : Md3Theme.colorScheme.surfaceContainerLow
                                border.width: 1
                                border.color: index === window.templateIndex
                                              ? Md3Theme.colorScheme.primary
                                              : Md3Theme.colorScheme.outlineVariant

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.title
                                        color: Md3Theme.colorScheme.colorOnSurface
                                        font.family: Md3Theme.typography.fontFamily
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.sub
                                        color: Md3Theme.colorScheme.colorOnSurfaceVariant
                                        font.family: Md3Theme.typography.fontFamily
                                        font.pixelSize: 11
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: window.templateIndex = index
                                }
                            }
                        }
                    }

                    // 2 Qt kits — multi-select
                    ColumnLayout {
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Md3TextField {
                                id: qtRootField
                                Layout.fillWidth: true
                                label: qsTr("Qt 根目录")
                                text: ProjectGenerator.qtRoot
                                onTextChanged: ProjectGenerator.qtRoot = text
                            }
                            Md3IconButton {
                                icon: "refresh"
                                onClicked: {
                                    ProjectGenerator.refreshKits()
                                    window.selectPreferredKits()
                                }
                            }
                            Md3IconButton {
                                icon: "folder_open"
                                accessibleName: qsTr("浏览 Qt 根目录")
                                onClicked: {
                                    const d = ProjectGenerator.pickDirectory(qsTr("Qt 根目录"), qtRootField.text)
                                    if (d) {
                                        qtRootField.text = d
                                        ProjectGenerator.qtRoot = d
                                        window.selectPreferredKits()
                                    }
                                }
                            }
                            Md3Button {
                                text: qsTr("添加")
                                variant: Md3Button.Outlined
                                onClicked: {
                                    const d = ProjectGenerator.pickDirectory(
                                                qsTr("选择 Qt Kit 前缀（如 …/6.10.2/mingw_64）"),
                                                ProjectGenerator.qtRoot)
                                    if (!d)
                                        return
                                    if (ProjectGenerator.addCustomKit(d)) {
                                        window.selectedKitIndexes =
                                                window.selectedKitIndexes.concat([ProjectGenerator.kits.length - 1])
                                        snack.show(qsTr("已添加自定义 Kit"))
                                    } else {
                                        snack.show(ProjectGenerator.lastError || qsTr("添加失败"))
                                    }
                                }
                            }
                        }
                        Text {
                            text: qsTr("点击多选编译器（已选 %1）").arg(window.selectedKitIndexes.length)
                            color: Md3Theme.colorScheme.colorOnSurfaceVariant
                            font.family: Md3Theme.typography.fontFamily
                            font.pixelSize: 11
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Md3Theme.shape.small
                            color: Md3Theme.colorScheme.surfaceContainerLow
                            border.width: 1
                            border.color: Md3Theme.colorScheme.outlineVariant
                            clip: true
                            ListView {
                                anchors.fill: parent
                                anchors.margins: 4
                                clip: true
                                model: ProjectGenerator.kits
                                spacing: 2
                                delegate: Md3ListTile {
                                    required property var modelData
                                    required property int index
                                    width: ListView.view.width
                                    height: 40
                                    title: (window.isKitSelected(index) ? "✓ " : "") + modelData.label
                                    subtitle: modelData.prefix
                                    selected: window.isKitSelected(index)
                                    onClicked: window.toggleKit(index)
                                }
                                Text {
                                    visible: ProjectGenerator.kits.length === 0
                                    anchors.centerIn: parent
                                    text: qsTr("未找到 Kit，可点「添加」指定前缀")
                                    color: Md3Theme.colorScheme.colorOnSurfaceVariant
                                    font.family: Md3Theme.typography.fontFamily
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    // 3 library + options
                    ColumnLayout {
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Md3TextField {
                                id: md3Field
                                Layout.fillWidth: true
                                label: window.copyLibrary ? qsTr("Md3 预编译/构建目录")
                                                          : qsTr("Md3 库目录（外部引用）")
                                text: ProjectGenerator.md3Path
                                supportingText: ProjectGenerator.isValidMd3Path(text)
                                                 ? (window.copyLibrary
                                                    ? qsTr("只复制 .a/.lib + 头文件 → vendor/Md3")
                                                    : qsTr("路径有效"))
                                                 : (window.copyLibrary
                                                    ? qsTr("需含 libMd3.a/.lib（或源码旁的 build）")
                                                    : qsTr("需包含 CMakeLists.txt"))
                                error: text.length > 0 && !ProjectGenerator.isValidMd3Path(text)
                                onTextChanged: ProjectGenerator.md3Path = text
                            }
                            Md3IconButton {
                                icon: "folder_open"
                                onClicked: {
                                    const d = ProjectGenerator.pickDirectory(qsTr("Md3 库目录"), md3Field.text)
                                    if (d) {
                                        md3Field.text = d
                                        ProjectGenerator.md3Path = d
                                    }
                                }
                            }
                        }
                        RowLayout {
                            spacing: 16
                            Row {
                                spacing: 8
                                Md3Switch {
                                    id: copyLibSwitch
                                    checked: window.copyLibrary
                                    onCheckedChanged: window.copyLibrary = checked
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("复制预编译库进项目")
                                    color: Md3Theme.colorScheme.colorOnSurface
                                    font.family: Md3Theme.typography.fontFamily
                                    font.pixelSize: 12
                                }
                            }
                            Row {
                                spacing: 8
                                visible: !window.copyLibrary
                                Md3Switch {
                                    id: absSwitch
                                    checked: window.md3Absolute
                                    onCheckedChanged: window.md3Absolute = checked
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("绝对路径引用")
                                    color: Md3Theme.colorScheme.colorOnSurface
                                    font.family: Md3Theme.typography.fontFamily
                                    font.pixelSize: 12
                                }
                            }
                            Md3TextField {
                                id: seedField
                                Layout.preferredWidth: 110
                                label: qsTr("种子色")
                                text: "#6750A4"
                            }
                        }
                        RowLayout {
                            spacing: 20
                            Row {
                                spacing: 8
                                Md3Switch { id: darkSwitch }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("深色")
                                    color: Md3Theme.colorScheme.colorOnSurface
                                    font.family: Md3Theme.typography.fontFamily
                                }
                            }
                            Row {
                                spacing: 8
                                Md3Switch {
                                    id: forceSwitch
                                    onCheckedChanged: window.refreshResolvedName()
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("覆盖同名目录")
                                    color: Md3Theme.colorScheme.colorOnSurface
                                    font.family: Md3Theme.typography.fontFamily
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: qsTr("%1  ·  %2  ·  %3  ·  %4")
                                  .arg(window.resolvedName)
                                  .arg(templateIds[templateIndex])
                                  .arg(window.selectedKitsLabel())
                                  .arg(window.copyLibrary ? qsTr("库→vendor") : qsTr("外链库"))
                            color: Md3Theme.colorScheme.colorOnSurfaceVariant
                            font.family: Md3Theme.typography.fontFamily
                            font.pixelSize: 11
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                // Done
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.step >= window.stepCount
                    spacing: 8
                    Item { Layout.fillHeight: true }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("项目已创建")
                        color: Md3Theme.colorScheme.primary
                        font.family: Md3Theme.typography.fontFamily
                        font.pixelSize: Md3Theme.typography.titleMedium.size
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: ProjectGenerator.lastOutput
                        color: Md3Theme.colorScheme.colorOnSurfaceVariant
                        font.pixelSize: 11
                        font.family: Md3Theme.typography.fontFamily
                    }
                    Item { Layout.fillHeight: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Md3Button {
                        visible: window.step < window.stepCount
                        text: qsTr("上一步")
                        variant: Md3Button.Text
                        enabled: window.step > 0
                        onClicked: window.goBack()
                    }
                    Item { Layout.fillWidth: true }
                    Md3Button {
                        visible: window.step >= window.stepCount
                        text: qsTr("打开目录")
                        variant: Md3Button.Outlined
                        onClicked: Qt.openUrlExternally(
                            "file:///" + ProjectGenerator.lastOutput.replace(/\\/g, "/"))
                    }
                    Md3Button {
                        visible: window.step >= window.stepCount
                        text: qsTr("再建一个")
                        variant: Md3Button.Filled
                        onClicked: window.step = 0
                    }
                    Md3Button {
                        visible: window.step < window.stepCount
                        text: window.step === window.stepCount - 1 ? qsTr("创建") : qsTr("下一步")
                        variant: Md3Button.Filled
                        enabled: !ProjectGenerator.busy && window.canNext()
                        onClicked: window.goNext()
                    }
                }
            }
        }

        Md3Snackbar {
            id: snack
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            z: 100
        }
    }
}
