**Archived**: This repository is archived and not under development. It's been replaced by [this implementation](https://github.com/tracktwo/unrealscript-debugger).

# DAP UnrealScript Debugger

This project is a debugger for UnrealScript based on the Debug Adapter Protocol used by Visual Studio Code and other editors. It integrates directly
with a [supported editor](https://microsoft.github.io/debug-adapter-protocol/implementors/tools/) instead of running in a dedicated window.

## Advantages

By running directly in your editor of choice, this debugger lets you:

- Use familiar keybindings to navigate source code instead of learning a new set supported by a dedicated debugger
- Easily edit code during debugging sessions (although any changes will not be reflected in the running session)
- Take advantage of many features the editor may support such as data visualization when hovering over identifiers, run to cursor, etc.

## Installation

The binary installation package contains several pieces to install, and instructions will depend on which editor you are using.

### Install the Debugger Interface

The debugger interface is required for all editors, and allows Unreal to talk to the debugger. The file `DebuggerInterface.dll` must be copied
to the binaries folder of the Unreal 3.x game you wish to debug. It should sit next to the binary that implements the game itself.

### Install the Adapter Plugin (Visual Studio Code)

`unrealscript-debuger-x.y` is a folder implementing a VS Code extension for the debugger and should be copied into your `.vscode/extensions` folder.
Once this is installed and the editor restarted, you can create a `launch.json` launch configuration for your project. This configuration tells the
editor which debugger to connect to and any other data it needs to pass along to it. A sample `launch.json` file is provided below:

```json
{
	"version": "0.2.0",
	"configurations": [
		{
			"type": "unrealscript",
			"request": "attach",
			"name": "attach debugger",
			"sourceRoots": [
				"C:\\Users\\MyUsername\\Documents\\mods\\mymod_project\\MyMod\\Src",
				"C:\\Program Files (x86)\\Steam\\steamapps\\common\\XCOM 2 War of the Chosen SDK\\Development\\Src"
			]
		}
	]
}
```

- `"type": "unrealscript"` line identifies this project as using the UnrealScript debugger.
- `"request": "attach"` indicates that we will be attaching to a running game rather than launching it. This is currently the only supported request type.
- `"sourceRoots"` is an array of strings that lists the locations of UnrealScript source files, in order of preference. When Unreal sends a
source location (such as when stopping at a breakpoint) it sends the class name and line, but mapping that class name back to a source file
must be done by the debugger. The debugger will search these directories in order looking for class names that match the name indicated by
Unreal, and the first matching file will appear in the editor. The directories named in this list should name the `Src` directory. There should
be directories under this directory with names matching unreal package(s) for the project, and under each of these directories a `Classes` directory
which contains the actual `.uc` source files.

At least one `sourceRoots` entry is required for the debugger to run. It will stop with an error dialog if no source roots are provided.

Note: if your development workflow involves copying unrealscript source files from a separate workspace into the Unreal `Development` tree before compiling,
ensure your workspace folders appear before the Unreal development tree in the source roots list. If the debugger locates files in the Unreal development
tree first it will open those files in the editor, and any changes accidentally made to files in that tree may be overwritten by the next build that copies
files from your workspace.

### Install the Adapter Plugin (Other Editors)

If you are using another editor, installation of the plugin may be different. The debug adapter is in a file called `DebugAdapter.exe`, but it's
editor-specific how to get your editor to talk to that debug adapter.

## Building from Source

Buliding this project from source has several dependencies:

- Visual Studio 2019 (Community edition is fine). Ensure the C++ workflow and CMake support is installed.
- Boost (headers only)
- cppdap (included as a submodule in the source repo)

Boost may be installed via [vcpkg](https://github.com/microsoft/vcpkg).

Open the root source folder in Visual Studio, and it should automatically detect the CMake config files. If Boost is installed via vcpkg, it should also detect your
vcpkg installation root automatically and locate the required boost headers.

