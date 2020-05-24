import * as vscode from 'vscode';

// Register the install interface command.
export function activate(context: vscode.ExtensionContext) {
    console.log("Debugger is active");
    let disposable = vscode.commands.registerCommand('extension.unrealscript-debugger-install-interface', installInterface);
    context.subscriptions.push(disposable);
}

export function deactivate() {}

// Copy the debugger folder interface DLL from the given subpath within the extension to the given destination
// folder.
function copyInterfaceFile(folder : vscode.Uri, interfacePath : string) {
    let extensionfolder = vscode.extensions.getExtension("x2-community-project.unrealscript-debugger").extensionPath;
    let source = vscode.Uri.file(extensionfolder + "/" + interfacePath +  "DebuggerInterface.dll");
    let destination = vscode.Uri.file(folder.path + "/DebuggerInterface.dll");
    vscode.workspace.fs.copy(source, destination, { overwrite: true }).then(
        () => {
            vscode.window.showInformationMessage("Unrealscript debugger interface installed");
        },
        (reason) => {
            vscode.window.showInformationMessage("Unrealscript debugger interface installation failed: " + reason);
        }
    );
}

// Show an open dialog to pick a destination folder for the debugger interface. Returns a promise for
// the selected folder.
function chooseGameFolder() {
    return vscode.window.showOpenDialog({
        "canSelectFiles": false,
        "canSelectFolders": true,
        "canSelectMany": false,
        "openLabel": "Choose game binary folder"
    });
}

// Entry point to copy the debugger interface DLL into a game folder. First prompts the user for
// 32 or 64 bit, then opens an open dialog box to choose the destination directory. Finally copies
// the file to that directory.
function installInterface() {
    vscode.window.showQuickPick(
        [
          { "label": "Install 32-bit debugger",  "interfacePath": "interface/win32/" },
          { "label": "Install 64-bit debugger", "interfacePath": "interface/win64/" }
        ],
        { canPickMany: false }
    ).then( (pick) => {
            if (pick)
                chooseGameFolder().then( (folders) => {
                    if (folders)
                        copyInterfaceFile(folders[0], pick.interfacePath);
                });
    });
}
