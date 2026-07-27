import QtQuick
import QtQuick.Layouts
import Md3

Md3ApplicationWindow {
    id: window
    width: 680
    height: 520
    minimumWidth: 560
    minimumHeight: 420
    maximumWidth: 800
    maximumHeight: 720
    visible: true
    title: qsTr("新建 Md3 项目")
    color: Md3Theme.colorScheme.surface
    roundedCorners: true
    cornerRadius: Md3WindowCapabilities.windowCornerRadius

    Component.onCompleted: {
        Md3Theme.applySeed("#6750A4")
        reconcileKitSelection()
    }

    Connections {
        target: ProjectGenerator
        function onKitsChanged() {
            window.reconcileKitSelection()
        }
    }

    property int step: 0
    readonly property int stepCount: 4
    property var selectedKitIndexes: []
    property int templateIndex: 1
    property bool md3Absolute: false
    property bool copyLibrary: true
    property string md3Linkage: "auto" // auto | shared | static
    property string buildType: "Release" // Debug | Release | RelWithDebInfo | MinSizeRel
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

    function md3LinkageToIndex(v) {
        if (v === "auto")
            return 0
        if (v === "shared")
            return 1
        return 2
    }

    function indexToMd3Linkage(i) {
        if (i === 0)
            return "auto"
        if (i === 1)
            return "shared"
        return "static"
    }

    function isMingwKitName(kit) {
        return String(kit).toLowerCase().indexOf("mingw") >= 0
    }

    function reconcileKitSelection() {
        const kits = ProjectGenerator.kits
        if (kits.length === 0) {
            selectedKitIndexes = []
            return
        }
        const selectedPrefixes = []
        for (let i = 0; i < selectedKitIndexes.length; ++i) {
            const idx = selectedKitIndexes[i]
            if (idx >= 0 && idx < kits.length)
                selectedPrefixes.push(kits[idx].prefix)
        }
        const newIndexes = []
        for (let p = 0; p < selectedPrefixes.length; ++p) {
            for (let i = 0; i < kits.length; ++i) {
                if (kits[i].prefix === selectedPrefixes[p]) {
                    newIndexes.push(i)
                    break
                }
            }
        }
        if (newIndexes.length === 0)
            selectPreferredKits()
        else
            selectedKitIndexes = newIndexes
    }

    function selectPreferredKits() {
        const kits = ProjectGenerator.kits
        if (kits.length === 0) {
            selectedKitIndexes = []
            return
        }
        const onWindows = Qt.platform.os === "windows"
        let idx = -1
        // Prefer 6.10 + platform toolchain (MSVC/Clang on Windows, gcc/linux on Linux)
        for (let i = 0; i < kits.length; ++i) {
            const k = kits[i]
            const ver = String(k.version)
            const kit = String(k.kit).toLowerCase()
            if (ver.indexOf("6.10") !== 0 && ver.indexOf("6.8") !== 0 && ver.indexOf("6.9") !== 0)
                continue
            if (onWindows && isMingwKitName(kit))
                continue
            if (onWindows && (kit.indexOf("msvc") >= 0 || kit.indexOf("clang") >= 0)) {
                idx = i
                break
            }
            if (!onWindows && (kit.indexOf("gcc") >= 0 || kit.indexOf("linux") >= 0
                               || kit.indexOf("clang") >= 0 || kit === "usr")) {
                idx = i
                break
            }
        }
        // Any Qt 6.x, still skip MinGW on Windows unless it is the only kit
        if (idx < 0) {
            for (let i = 0; i < kits.length; ++i) {
                const kit = String(kits[i].kit).toLowerCase()
                if (onWindows && isMingwKitName(kit))
                    continue
                if (String(kits[i].version).indexOf("6.") === 0
                        || String(kits[i].version) === "system") {
                    idx = i
                    break
                }
            }
        }
        if (idx < 0) {
            for (let i = 0; i < kits.length; ++i) {
                if (!onWindows || !isMingwKitName(kits[i].kit)) {
                    idx = i
                    break
                }
            }
        }
        if (idx < 0)
            idx = 0
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
            md3Linkage: md3Linkage,
            buildType: buildType,
            vendorFolder: "Md3",
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

                // StackLayout sizes to the tallest step and can push footer buttons off-screen.
                Item {
                    id: stepHost
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    visible: window.step < window.stepCount

                    // 0 project — horizontal fields
                    ColumnLayout {
                        anchors.fill: parent
                        visible: window.step === 0
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

                    // 1 template — text on top, preview below
                    RowLayout {
                        anchors.fill: parent
                        visible: window.step === 1
                        spacing: 8
                        Repeater {
                            model: [
                                { title: qsTr("空白"), sub: qsTr("空窗口"), kind: "empty" },
                                { title: qsTr("基础"), sub: qsTr("按钮示例"), kind: "basic" },
                                { title: qsTr("导航"), sub: qsTr("Rail 多页"), kind: "rail" }
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

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            width: parent.width
                                            horizontalAlignment: Text.AlignHCenter
                                            text: modelData.title
                                            color: Md3Theme.colorScheme.colorOnSurface
                                            font.family: Md3Theme.typography.fontFamily
                                            font.pixelSize: 15
                                            font.weight: Font.Medium
                                        }
                                        Text {
                                            width: parent.width
                                            horizontalAlignment: Text.AlignHCenter
                                            text: modelData.sub
                                            color: Md3Theme.colorScheme.colorOnSurfaceVariant
                                            font.family: Md3Theme.typography.fontFamily
                                            font.pixelSize: 11
                                        }
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.minimumHeight: 72

                                        // Mini window preview
                                        Rectangle {
                                            id: previewFrame
                                            anchors.centerIn: parent
                                            width: Math.min(parent.width - 4, 112)
                                            height: Math.min(parent.height - 4, 80)
                                            radius: 6
                                            color: Md3Theme.colorScheme.surface
                                            border.width: 1
                                            border.color: Md3Theme.colorScheme.outlineVariant

                                            Rectangle {
                                                anchors.top: parent.top
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                height: 14
                                                radius: 6
                                                color: Md3Theme.colorScheme.surfaceContainerHigh
                                                Rectangle {
                                                    anchors.bottom: parent.bottom
                                                    width: parent.width
                                                    height: parent.height / 2
                                                    color: parent.color
                                                }
                                                Row {
                                                    anchors.left: parent.left
                                                    anchors.leftMargin: 5
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    spacing: 3
                                                    Repeater {
                                                        model: 3
                                                        Rectangle {
                                                            width: 4
                                                            height: 4
                                                            radius: 2
                                                            color: Md3Theme.colorScheme.outline
                                                        }
                                                    }
                                                }
                                            }

                                            // empty — blank content
                                            Item {
                                                visible: modelData.kind === "empty"
                                                anchors.fill: parent
                                                anchors.topMargin: 14
                                            }

                                            // basic — centered button
                                            Rectangle {
                                                visible: modelData.kind === "basic"
                                                anchors.centerIn: parent
                                                anchors.verticalCenterOffset: 6
                                                width: 44
                                                height: 16
                                                radius: 8
                                                color: Md3Theme.colorScheme.primary
                                            }

                                            // rail — side nav + content
                                            Row {
                                                visible: modelData.kind === "rail"
                                                anchors.fill: parent
                                                anchors.topMargin: 14
                                                spacing: 0
                                                Rectangle {
                                                    width: previewFrame.width * 0.28
                                                    height: previewFrame.height - 14
                                                    color: Md3Theme.colorScheme.surfaceContainerLow
                                                    Column {
                                                        anchors.centerIn: parent
                                                        spacing: 4
                                                        Repeater {
                                                            model: 3
                                                            Rectangle {
                                                                width: 10
                                                                height: 10
                                                                radius: 5
                                                                color: index === 0
                                                                       ? Md3Theme.colorScheme.primary
                                                                       : Md3Theme.colorScheme.outlineVariant
                                                            }
                                                        }
                                                    }
                                                }
                                                Rectangle {
                                                    width: previewFrame.width * 0.72 - 1
                                                    height: previewFrame.height - 14
                                                    color: "transparent"
                                                    Rectangle {
                                                        anchors.centerIn: parent
                                                        width: parent.width * 0.55
                                                        height: 8
                                                        radius: 4
                                                        color: Md3Theme.colorScheme.surfaceContainerHighest
                                                    }
                                                }
                                            }
                                        }
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
                        anchors.fill: parent
                        visible: window.step === 2
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
                                                qsTr("选择 Qt Kit 前缀（如 …/6.10.2/gcc_64 或 /usr）"),
                                                ProjectGenerator.qtRoot)
                                    if (!d)
                                        return
                                    if (ProjectGenerator.addCustomKit(d)) {
                                        window.selectedKitIndexes = [ProjectGenerator.kits.length - 1]
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
                                    text: qsTr("未找到 Kit：检查 Qt 根目录，或点「添加」指定 …/gcc_64 或 /usr")
                                    color: Md3Theme.colorScheme.colorOnSurfaceVariant
                                    font.family: Md3Theme.typography.fontFamily
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    // 3 library + options
                    ColumnLayout {
                        anchors.fill: parent
                        visible: window.step === 3
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Md3TextField {
                                id: md3Field
                                Layout.fillWidth: true
                                label: window.copyLibrary ? qsTr("同目录 Md3 包（将复制为 ./Md3）")
                                                          : qsTr("Md3 库目录（外部引用）")
                                text: ProjectGenerator.md3Path
                                supportingText: ProjectGenerator.isValidMd3Path(text)
                                                 ? (window.copyLibrary
                                                    ? qsTr("固定复制到工程同目录 Md3/")
                                                    : qsTr("路径有效"))
                                                 : (window.copyLibrary
                                                    ? qsTr("需为 Md3Create 旁的 Md3/ 包或含 libMd3 的目录")
                                                    : qsTr("需包含 CMakeLists.txt 或已打包的 Md3"))
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
                            spacing: 8
                            Text {
                                text: qsTr("Md3 链接方式")
                                color: Md3Theme.colorScheme.colorOnSurfaceVariant
                                font.family: Md3Theme.typography.fontFamily
                                font.pixelSize: 12
                            }
                            Md3SegmentedButton {
                                Layout.fillWidth: true
                                model: [
                                    { text: qsTr("自动") },
                                    { text: qsTr("动态库") },
                                    { text: qsTr("静态库") }
                                ]
                                currentIndex: md3LinkageToIndex(window.md3Linkage)
                                onSelectionChanged: window.md3Linkage = indexToMd3Linkage(currentIndex)
                            }
                        }
                        RowLayout {
                            spacing: 16
                            Text {
                                text: qsTr("运行方式")
                                color: Md3Theme.colorScheme.colorOnSurfaceVariant
                                font.family: Md3Theme.typography.fontFamily
                                font.pixelSize: 12
                            }
                            Item { Layout.fillWidth: true }
                            Row {
                                spacing: 8
                                Md3Switch {
                                    checked: window.buildType === "Release"
                                    onToggled: function (on) {
                                        if (on)
                                            window.buildType = "Release"
                                        else if (window.buildType === "Release")
                                            window.buildType = "Debug"
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("Release")
                                    color: window.buildType === "Release"
                                           ? Md3Theme.colorScheme.colorOnSurface
                                           : Md3Theme.colorScheme.colorOnSurfaceVariant
                                    font.family: Md3Theme.typography.fontFamily
                                    font.pixelSize: 12
                                }
                            }
                            Row {
                                spacing: 8
                                Md3Switch {
                                    checked: window.buildType === "Debug"
                                    onToggled: function (on) {
                                        if (on)
                                            window.buildType = "Debug"
                                        else if (window.buildType === "Debug")
                                            window.buildType = "Release"
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("Debug")
                                    color: window.buildType === "Debug"
                                           ? Md3Theme.colorScheme.colorOnSurface
                                           : Md3Theme.colorScheme.colorOnSurfaceVariant
                                    font.family: Md3Theme.typography.fontFamily
                                    font.pixelSize: 12
                                }
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
                                  .arg((window.copyLibrary ? qsTr("库→./Md3") : qsTr("外链库"))
                                       + " / "
                                       + (window.md3Linkage === "auto" ? qsTr("自动")
                                          : (window.md3Linkage === "shared" ? qsTr("动态")
                                             : qsTr("静态")))
                                  + " / "
                                  + window.buildType)
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
                    Layout.preferredHeight: 40
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
