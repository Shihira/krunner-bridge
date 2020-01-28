# KRunner Bridge

TODOS:
- make some parts of plugin more reusable
- Use signals for processes
- Add debug option

**Write krunner python plugins the quick way.**

<del>Plasma 5 stopped exposing KDE Framework API to script languages (except QML) for the purpose of so-called stability (but it is not at all stable, crashing from time to time). But this brings some horrible consequence that writing krunner plugins becomes time-consuming. You have to write long C++ code, configure your C++ building toolchain, and compile them each time you modify something.</del>

No more pains with ONE plugin for all.

-----

UPDATE: A KDE maintainer has described a way that was newly introduced in KDE 5.11, to make use of DBus to achieve this. See this post: http://blog.davidedmundson.co.uk/blog/cross-process-runners/  
There are also (unofficial) KDevelop templates for this: https://www.pling.com/c/1355251/

## Installation

Requirements:

* Python 3.x
* KF5 & Qt5

Debian/Ubuntu:  
`sudo apt install cmake extra-cmake-modules build-essential libkf5runner-dev libkf5textwidgets-dev qtdeclarative5-dev gettext libnotify-bin`

openSUSE:  
`sudo zypper install cmake extra-cmake-modules libQt5Widgets5 libQt5Core5 libqt5-qtlocation-devel ki18n-devel
ktextwidgets-devel kservice-devel krunner-devel gettext-tools libnotify-tools`  

Fedora:  
`sudo dnf install cmake extra-cmake-modules kf5-ki18n-devel kf5-kservice-devel kf5-krunner-devel kf5-ktextwidgets-devel gettext libnotify`  

Clone this repository to local filesystem and cd into it.

```sh
./install.sh
```

## Quick Start

By executing the installer you get a basic calculator using python's eval function at `~/.local/share/kservices5/example_calc.py`:

```python
#!/usr/bin/env python3

import krunner_bridge
from math import *

@krunner_bridge.match_handler
def match(query):
    try:
        res = eval(query)
        if isinstance(res, (int, float)):
            return krunner_bridge.datasource(
                text=str(res),
                icon="accessories-calculator",
                category="Python Calculator")
    except:
        pass

if __name__ == "__main__":
    krunner_bridge.exec()
```


All stderr output from your python script will be forwarded and displayed in the console.
That's what it will be like when it is installed:

![](preview/screenshot-1.png)

For more practical examples, check out `example_search.py`. It searches in my useful little scripts and applications, with far better fuzzy match algorithm and selection mechanism. As what shows below, when you type one more key `5` to form a query `kse5`, the 5th choice `KColorSchemeEditor` will automatically be selected.

![](preview/screenshot-2.png)

## Configuration

### Timeouts
In the `krunner_bridge.desktop` file are already the config entries for the timeouts configured, if you want them
to take indefinitely long, you can set the value to -1.

### Simple Configuration
You can add python3 scripts by adding `X-KRunner-Bridge-Script###=<path/to/your/script>`, 
to the `/.local/share/kservices5/krunner_bridge.desktop` file where ### can be a number, an ID or anything unique.
This way the script is called for init, match and run operations. This is simple but it can be far more efficient to use the config groups.

### Config Groups
With the config groups you can specify more parameters, an example entry looks like this:  
```
[X-KRunner-Bridge-Script-Config-3]  
FilePath=debug_helper.sh
LaunchCommand=bash
MatchRegex=^hello
Initialize=true
Prepare=true
Teardown=true
RunDetached=true
```
In the first line with in the [] you specify the name of the group, this has to start with `X-KRunner-Bridge-Script` and has to be unique.
Firstly you have to specify the file path, this path can be relative to `~/.local/share/kservices5/` or absolute.
All other keys are optional.
The `LaunchCommad` is the command that gets executed when executing the script, it defaults to `python3`.  
The `MatchRegex` is a regular expression which determines if the match operation should be with the current query executed.  
In this example the value is `^hello`, this means that the query has to start with `hello`, if not the script does not get called.
By using this you can avoid a lot of unnecessary plugin executions. It defaults to `.*` which always calls the script.  
If you want to test your regular expressions https://regex101.com/ can be very useful.

The `Initialize`, `Setup` and `Teardown` keys determine which lifecycle methods should be called. The Setup and Teardown keys refer
to the KRunner match session, for instance the setup operation gets called when a new KRunner match session is started (before text is typed).
By default only init is enabled.

The `RunDetached` key determines, if the process calling the run method should be started attached or detached,
by default it is detached. If set to false you can set a timeout as described above.

## API

* `exec()`: The entry of a script
* `match_handler`: Register a function having parameter `query` and returning a list of `datasource` as a match handler
* `init_handler`: Register a function having no parameter as an init handler
* `run_handler`: Register a function having parameter `data` as a run handler
* `setup_handler`: Register a function having no parameter as an setup handler. This function will be called when a new match session is started.
* `teardown_handler`: Register a function having no parameter as an teardown handler. This function will be called when a match session ended.
* `datasource`
    - Properties:
        - data: Save a dict/array/string/numeric here for later use (in run handler)
        - text: Display text
        - cmp: String for matcher comparison
        - icon: Icon name. check them in `kdialog --geticon`
        - category: Display category text
        - relevance: For result sorting in krunner
    - Methods:
        - `from_files(glob_pattern, icon, cat="")`: Collect files matching glob_pattern
        - `from_desktop(glob_pattern, cat="")`: Collect applications matching glob_pattern, icons are extracted from desktop files
        - `fuzzy_match(query_string)`: Better matching algorithm. Returns a bool.

## FAQs

* Q: How does it work actually?
* A: The C++ plugin just calls python scripts throw spawning subprocess, and passes parameters like `{"operation":"query","query":"kss"}` through standard input, while the script parses this and returns the result set as an launch parameter, formatted in JSON likewise. This mean you can write the script in any language (even bash) as long as you can parse JSON. To note that the script is called everytime there is an action (run/query/init/setup/teardown), so make your script finished as quickly as possible, and do not keep alive in background.

* Q: What if I want multiple scripts?
* A: Just append one more config entry (line or group), where the key anything unique.
KRunner bridge searches all properties and groups starting with `X-KRunner-Bridge-Script`.

-----

* Q: Is it possible to move my scripts somewhere else?
* A: Yes. Scripts are not required to be in the services directory, for this is just a default searching path. Just specify the right path, and remember to quote special characters.

-----

* Q: How can I make some cache to speed up searching?
* A: Do initialization and slow tasks in `init`, And handle caches by your own. For example, `pickle` your data up and save them in `/tmp`. `datasource` the python class provide some method to generate results, which might be useful.  
Additionally you can use the config groups instead of a single config line to provide additional information, for instance a regular expression.
The Configuration section explains this in detail.

-----

* Q: I got an error like "The shared library krunner_bridge.so was not found.", how can I resolve it? (#2)
* A: This may be caused by the misconfigured Qt plugin search path. For user specific path it is usually set to `~/.local/lib/qt/plugins/` (as is done in the cmake build script) in most \*nix dists but some are not. If this problem comes to you, try to set this path to env-var by exporting `export QT_PLUGIN_PATH=~/.local/lib/qt/plugins/`. To ensure it works globally, add it to `.bashrc` or `~/.config/plasma-workspace/env/`.
