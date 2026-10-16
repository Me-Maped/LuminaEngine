"""Checks the editor's localization table against the strings its call sites use.

    python BuildScripts/CheckLocalization.py            report, exit 1 when anything is found
    python BuildScripts/CheckLocalization.py --quiet    summary only

Three checks, each of which has caught a real bug:

  1. TABLE INTEGRITY.  A translation has to keep exactly the placeholders its English source has.
     These strings go straight into printf and into the engine's own {} formatter, so a dropped
     %s or a changed argument type can crash or garble a line. Numbered brace fields may be
     reordered while retaining their argument contracts. This check is not negotiable.

  2. USED BUT MISSING.  Every literal handed to LOC/LOC_ID/LOCF/... needs an entry. A missing
     entry still renders -- the source string is the fallback -- which is exactly why they
     accumulate without anyone noticing.

  3. UNWRAPPED DISPLAY LITERALS.  A string literal that ImGui draws without going through one of
     the LOC macros. This is the reverse direction, and it is the one that matters most: the mesh
     import dialogue (2200 lines) and the component visualizer panels (1700 lines) were both
     entirely English because only direction 2 was ever checked.

  4. VIEW-REACHED NAMES.  A pin name, a node's display name or a node's category is drawn through
     a variable (LOC_VIEW(pin->GetPinName()), LOC_VIEW_CTX(node->GetNodeDisplayName(), "node")),
     so no call-site scan can see the literal and the reverse scan cannot either -- the literal is
     not drawn where it is written. That is how 174 of 251 node names, 38 pin names and 14 of 31
     categories were shipping in English while the table looked complete. What is checkable is the
     declaration site, so that is what this checks: a literal handed to one of those providers has
     to have an entry.

Deliberately not translated, and listed in SKIP_* below: vendored trees, the LLM protocol text
under Plugins/MCP, brand and platform names, code samples drawn as help text, and the copyable
memory report. Each entry there carries its reason, so adding one is a decision rather than an
oversight.
"""

import json
import os
import re
import sys

from LocalizationFormat import have_compatible_format_arguments

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = "Engine/Resources/Localization/zh-Hans.json"

SCAN_DIRS = ["Engine/Editor/Source", "Engine/Source/Runtime/Source", "Engine/Plugins"]

# Vendored code, generated code and the MCP protocol, none of which is ours to localize.
# The NsightPerf plugin needs a closed-source SDK that is not vendored, so it is not built here.
SKIP_DIRS = ("ThirdParty", "imgui-node-editor", "imgui_", "/MCP/", "/NsightPerf/", "/generated/")

# Whole files whose drawn text is deliberately not translated. Keyed by source-relative path.
SKIP_FILES = {
    "Engine/Source/Runtime/Source/Tools/UI/ImGui/GraphEditor.cpp":
        "vendored node-graph editor (MIT, Cedric Guillemet); its EditOptions() has no caller here",
    "Engine/Editor/Source/UI/Tools/Debug/AboutEditorTool.cpp":
        "Vulkan / Windows / Linux / macOS / MSVC / Clang / GCC -- API, platform and toolchain names",
    "Engine/Source/Runtime/Source/Core/Math/Math.cpp":
        "formatted for the log, not for a panel",
    "Engine/Source/Runtime/Source/World/Entity/Systems/AnimationSystem.cpp":
        "formatted for the log, not for a panel",
    "Engine/Editor/Source/UI/Tools/AssetEditors/DataTable/DataTableWidgets.cpp":
        "C++ template shown verbatim as the empty-state help",
    "Engine/Editor/Source/UI/Tools/AssetEditors/RmlUiEditor/RmlUiEditorData.cpp":
        "generated RML source shown verbatim",
    "Engine/Source/Runtime/Source/Tools/UI/ImGui/ImGuiMemoryEditor.h":
        "vendored memory editor (MIT, ocornut/imgui_club)",
}

# Individual literals that are drawn but deliberately left alone. Keyed by the literal itself;
# use "path|literal" to exempt one only where it is a data value rather than a label.
SKIP_LITERALS = {
    "  %dx%d": "terminal cell geometry, no prose",
    "RML/RCSS": "language name, shown next to the byte count",
    "{}##skey": "an ImGui ID suffix; the visible half in front of it is already localized",

    # Per-site, by "<path>|<literal>".
    "Engine/Editor/Source/UI/EditorUI.cpp|EditorDockSpaceWindow":
        "carries ImGuiWindowFlags_NoTitleBar, so the name is only ever an ID",
    "Engine/Source/Runtime/Source/Tools/UI/ImGui/Widgets/TreeListView.cpp|Label":
        "the table never calls TableHeadersRow(), so the column name is only ever an ID",
    "Engine/Editor/Source/UI/Tools/AssetEditors/RmlUiEditor/RmlUiEditorTool.cpp|"
    "decorator: image( url );\ndecorator: gradient( vertical #1f2a36 #0e1620 );\n"
    "decorator: tiled-box( ... );\nfont-effect: outline(1dp #000);\n"
    "font-effect: shadow(0dp 1dp #000a);\nfont-effect: glow(2dp 0dp 0dp #4af);":
        "RCSS property list shown verbatim as the help text",
}

# The declaration sites of a name that only ever reaches the table through a variable, with the
# reader-facing name of each so a finding says which family is short of an entry.
VIEW_PROVIDERS = [
    (re.compile(r'GetNodeDisplayName\(\)\s*const\s*override\s*\{\s*return\s*"((?:[^"\\]|\\.)*)"'),
     "node display name"),
    (re.compile(r'GetNodeCategory\(\)\s*const\s*override\s*\{\s*return\s*"((?:[^"\\]|\\.)*)"'),
     "node category"),
    # CreateInputPin("Name", ...) and friends take the name first; CreatePin takes a pin class
    # first, so the name is the second argument there.
    (re.compile(r'(?:CreateInputPin|CreateOutputPin|CreateAnimPin|CreatePin|AddPin|SetPinName)\('
                r'\s*"((?:[^"\\]|\\.)*)"'),
     "pin name"),
    (re.compile(r'CreatePin\(\s*[^",][^,]*,\s*"((?:[^"\\]|\\.)*)"'),
     "pin name"),
]

# Argument index that ImGui actually draws. Only where it is not the first.
DISPLAY_CALLS = {
    "TextColored": 1, "LabelText": 1, "Value": 1, "Combo": 1, "TreeNodeEx": 1,
    "InputTextWithHint": 1, "ProgressBar": 2, "SliderFloat": 4, "SliderInt": 4,
    "DragFloat": 5, "DragInt": 5, "InputFloat": 4, "InputInt": 4,
}
for _name in ("Text", "TextUnformatted", "TextWrapped", "TextDisabled", "TextLink", "BulletText",
              "MenuItem", "Button", "SmallButton", "Selectable", "BeginMenu", "BeginTabItem",
              "RadioButton", "Checkbox", "BeginListBox", "ListBox", "TreeNode", "CollapsingHeader",
              "TableHeader", "TableSetupColumn", "BeginCombo", "BeginTooltip", "SetTooltip",
              "SetItemTooltip", "TextTooltip", "Begin"):
    DISPLAY_CALLS.setdefault(_name, 0)

MACROS = "LOC|LOC_ID|LOCF|LOC_VIEW|LOC_S"
LOC_CALL = re.compile(r'\b(?:' + MACROS + r')\(\s*(' + r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*' + r')')
LOC_ICON_CALL = re.compile(r'\b(?:LOC_ICON|LOC_ICON_ID|LOC_ICON_CTX)\(\s*[A-Za-z0-9_]+,\s*(' +
                           r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*' + r')')
SNAPSHOT_CALL = re.compile(r'\b(?:LOC_IN|LOCF_IN)\s*\(')
CTX_CALL = re.compile(r'\bLOC(?:_ICON)?_CTX\(\s*(?:[A-Za-z0-9_]+,\s*)?(' +
                      r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*' + r')\s*,\s*"((?:[^"\\]|\\.)*)"')

# A node's hover text, which is the second family of drawn text with no call site: GetNodeTooltip
# returns a view rather than handing a literal to a macro, so nothing else can see it. It is read
# as a *definition* instead of by the shape of the statement a literal sits in, because a tooltip
# is long prose and most are written as a multi-line concatenation whose first line reads only
# "return" -- a line-shaped rule would place the one-line ones and miss the rest.
NODE_TOOLTIP = re.compile(r'\bGetNodeTooltip\s*\(\s*\)\s*const')

# Coverage-only prose heuristic; table safety uses the full argument parser below.
PLACEHOLDER = re.compile(r'%(?:%|[-+ #0]*\d*(?:\.\d+)?[a-zA-Z])|\{\d*\}')

# What a literal means in every language: unit symbols, ratios and dimensions. A literal that is
# only these and format specifiers carries no prose, so it is not reported.
UNITS = {"B", "KB", "MB", "GB", "TB", "KiB", "MiB", "GiB", "px", "dp", "ms", "s", "x", "P",
         "F", "M", "K", "W", "Hz", "dB", "UV", "UV0", "UV1", "RGB", "RGBA"}

CPP_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "a": "\a", "b": "\b", "f": "\f",
               "v": "\v", "\\": "\\", '"': '"', "'": "'"}

CALL_RE = re.compile(r'(?:(?:ImGui|ImGuiX|Dialogs|Notifications|ImGuiX::Notifications)::)?\b(' +
                     "|".join(sorted(DISPLAY_CALLS, key=len, reverse=True)) + r')\s*\(')


def decode_cpp(literal_run):
    """The value a C++ literal run actually holds, so it can be compared with a JSON key."""
    raw = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', literal_run))
    return re.sub(r'\\(.)', lambda m: CPP_ESCAPES.get(m.group(1), m.group(1)), raw)


def cpp_string_at(text, index):
    """The source text of the literal whose opening quote is at index, and the index past it."""
    cursor = index + 1
    while cursor < len(text):
        if text[cursor] == "\\":
            cursor += 2
            continue
        cursor += 1
        if text[cursor - 1] == '"':
            break
    return text[index:cursor], cursor


def skip_whitespace(text, index):
    while index < len(text) and text[index] in " \t\r\n":
        index += 1
    return index


def balanced_block(text, index):
    """From an opening brace at index to its match, stepping over strings and char literals."""
    depth, cursor, quote = 0, index, None
    while cursor < len(text):
        char = text[cursor]
        if quote is not None:
            if char == "\\":
                cursor += 2
                continue
            if char == quote:
                quote = None
            cursor += 1
            continue
        if char in "\"'":
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[index:cursor + 1]
        cursor += 1
    return None


def node_tooltips(text):
    """(offset, literal) for every string a GetNodeTooltip override returns.

    A literal of None means the body returns something this reader cannot follow, which the caller
    reports rather than ignores: a tooltip the checker cannot read is a tooltip it cannot require a
    translation for, and silence there would look the same as coverage.
    """
    for definition in NODE_TOOLTIP.finditer(text):
        cursor = skip_whitespace(text, definition.end())
        if text.startswith("override", cursor):
            cursor = skip_whitespace(text, cursor + len("override"))
        # A declaration in a header has no body; the next brace in the file belongs to something
        # else, so the shape has to be checked before the brace is trusted.
        if cursor >= len(text) or text[cursor] != "{":
            continue
        body = balanced_block(text, cursor)
        if body is None:
            continue

        for statement in re.finditer(r'\breturn\b', body):
            cursor = skip_whitespace(body, statement.end())
            runs = []
            if cursor < len(body) and body[cursor] == '"':
                # Adjacent literals are one value to the compiler, so they are one key here.
                while cursor < len(body) and body[cursor] == '"':
                    run, cursor = cpp_string_at(body, cursor)
                    runs.append(run)
                    cursor = skip_whitespace(body, cursor)
            else:
                # A wrapped call, e.g. MaterialOutputNode returns LOC("The final output ...").
                call = re.match(r'[A-Za-z_][A-Za-z0-9_:]*\s*\(', body[cursor:])
                if call is not None:
                    inner = skip_whitespace(body, cursor + call.end())
                    if inner < len(body) and body[inner] == '"':
                        run, _ = cpp_string_at(body, inner)
                        runs.append(run)
            yield definition.start(), decode_cpp(" ".join(runs)) if runs else None


def strip_comments(text):
    """Blanks out comments and the inside of string literals, keeping every newline in place."""
    out, index, size = [], 0, len(text)
    while index < size:
        char = text[index]
        if char == "/" and text.startswith("//", index):
            end = text.find("\n", index)
            index = size if end < 0 else end
        elif char == "/" and text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = size if end < 0 else end + 2
            out.append("\n" * text.count("\n", index, end))
            index = end
        elif char in "\"'":
            # Copied through: a literal's contents are the thing being checked, and a // or /*
            # inside one is text rather than a comment. Character literals are handled the same way
            # -- one containing a quote would otherwise throw off every later literal in the file.
            # Copied through: a literal's contents are the thing being checked, and a // or /*
            # inside one is text rather than a comment.
            out.append(char)
            index += 1
            closer = char
            while index < size:
                if text[index] == "\\":
                    out.append(text[index:index + 2])
                    index += 2
                    continue
                out.append(text[index])
                index += 1
                if text[index - 1] == closer:
                    break
        else:
            out.append(char)
            index += 1
    return "".join(out)


def literals_at_top_level(argument):
    """The literals the argument hands to ImGui itself, not the ones inside a nested call.

    Returned as source text rather than decoded, so the concatenation of two adjacent literals
    still reads as one label. Anything inside a call is skipped -- ImGui only ever draws the
    argument it is handed, and a literal a helper built for it is that helper's business.
    """
    found, depth, index = [], 0, 0
    while index < len(argument):
        char = argument[index]
        if char == '"':
            start = index
            index += 1
            while index < len(argument):
                if argument[index] == "\\":
                    index += 2
                    continue
                index += 1
                if argument[index - 1] == '"':
                    break
            if depth == 0:
                found.append(argument[start:index])
            continue
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif char == "'":
            index += 1
            while index < len(argument) and argument[index] != "'":
                index += 1
        index += 1
    return found


def split_args(text, open_paren):
    """The call's arguments, split at depth 0."""
    args, current, depth, index = [], "", 1, open_paren + 1
    while index < len(text) and depth > 0:
        char = text[index]
        if char in "\"'":
            start, quote = index, char
            index += 1
            while index < len(text):
                if text[index] == "\\":
                    index += 2
                    continue
                index += 1
                if text[index - 1] == quote:
                    break
            current += text[start:index]
            continue
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                break
        elif char == "," and depth == 1:
            args.append(current)
            current = ""
            index += 1
            continue
        current += char
        index += 1
    args.append(current)
    return args


def carrier_text(literal):
    """The prose a literal carries, with format specifiers and units taken out."""
    words = [word for word in PLACEHOLDER.sub(" ", literal).split() if word not in UNITS]
    return " ".join(words)


def source_files():
    for scan_dir in SCAN_DIRS:
        base = os.path.join(ROOT, scan_dir)
        if not os.path.isdir(base):
            continue
        for dirpath, _, names in os.walk(base):
            normalised = dirpath.replace("\\", "/") + "/"
            if any(skip in normalised for skip in SKIP_DIRS):
                continue
            for name in names:
                if name.endswith((".cpp", ".h")):
                    yield os.path.join(dirpath, name), os.path.relpath(os.path.join(dirpath, name), ROOT).replace("\\", "/")


def format_source(key):
    """A lowercase trailing context tag is metadata, not formatted display text."""
    source, separator, context = key.rpartition("|")
    return source if separator and re.fullmatch(r"[a-z][a-z0-9_-]*", context) else key


def load_table():
    findings = []
    path = os.path.join(ROOT, TABLE)
    with open(path, "rb") as handle:
        raw = handle.read()
    if raw.startswith(b"\xef\xbb\xbf"):
        findings.append((TABLE, "has a UTF-8 BOM; the table is a plain UTF-8 document"))
    if raw.endswith(b"\n"):
        findings.append((TABLE, "ends with a newline; the table is written without one"))

    # A non-string value is a section marker the ordering tool leaves behind (see
    # LocalizationTable.py); FLocalization::ParseTable skips those on purpose, so the table is
    # the string entries only and the markers are not something this script has an opinion on.
    parsed = json.loads(raw.decode("utf-8"))
    table = {key: value for key, value in parsed.items() if isinstance(value, str)}
    markers = [key for key, value in parsed.items() if not isinstance(value, str)]
    for source, translation in table.items():
        if not have_compatible_format_arguments(format_source(source), translation):
            findings.append((TABLE, f"placeholder mismatch: {source!r} -> {translation!r}"))
        # A straight quote the source did not have reads as a typo in Chinese text. Where the
        # source itself carries one (embedded code) it is data and has to come through as-is.
        if '"' in translation and '"' not in source:
            findings.append((TABLE, f"straight quote introduced by a translation: {source!r}"))
    return table, findings, markers


def check_view_names(table):
    findings, missing = [], {}
    for full, relative in source_files():
        if relative in SKIP_FILES:
            continue
        try:
            text = open(full, encoding="utf-8-sig").read()
        except (UnicodeDecodeError, OSError):
            continue
        stripped = strip_comments(text)

        for pattern, family in VIEW_PROVIDERS:
            for match in pattern.finditer(stripped):
                literal = decode_cpp('"' + match.group(1) + '"')
                # An empty name is a pin that draws no label, and a token like "R" or "Layer0" is
                # kept in Latin -- that one is an entry in the table that says the same thing back,
                # rather than an exemption here, so this check stays a single rule.
                if not literal or literal in missing:
                    continue
                if literal not in table:
                    line = stripped.count("\n", 0, match.start()) + 1
                    missing[literal] = (f"{relative}:{line}", family)

    for literal in sorted(missing):
        where, family = missing[literal]
        findings.append((where, f"{family} has no entry: {literal!r}"))
    return findings


def check_node_tooltips(table):
    findings, missing, unreadable = [], {}, []
    for full, relative in source_files():
        if relative in SKIP_FILES:
            continue
        try:
            text = open(full, encoding="utf-8-sig").read()
        except (UnicodeDecodeError, OSError):
            continue
        stripped = strip_comments(text)

        for offset, literal in node_tooltips(stripped):
            line = stripped.count("\n", 0, offset) + 1
            if literal is None:
                unreadable.append(f"{relative}:{line}")
            elif literal and literal not in table and literal not in missing:
                missing[literal] = f"{relative}:{line}"

    for literal in sorted(missing):
        findings.append((missing[literal], f"node tooltip has no entry: {literal!r}"))
    for where in unreadable:
        findings.append((where, "node tooltip returns something this check cannot read"))
    return findings


def localization_literals(text):
    """Plain source literals consumed by localization macros, shared with the table ordering tool."""
    for pattern in (LOC_CALL, LOC_ICON_CALL):
        for match in pattern.finditer(text):
            yield decode_cpp(match.group(1))
    for match in SNAPSHOT_CALL.finditer(text):
        args = split_args(text, match.end() - 1)
        if len(args) > 1:
            literal = re.match(r'\s*("(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*)\s*$', args[1])
            if literal:
                yield decode_cpp(literal.group(1))


def check_call_sites(table):
    used, contextual, unwrapped = set(), set(), {}
    for full, relative in source_files():
        if relative in SKIP_FILES:
            continue
        try:
            with open(full, encoding="utf-8-sig") as handle:
                text = handle.read()
        except (UnicodeDecodeError, OSError):
            continue
        stripped = strip_comments(text)

        used.update(localization_literals(stripped))
        for match in CTX_CALL.finditer(stripped):
            contextual.add((decode_cpp(match.group(1)), match.group(2)))

        for match in CALL_RE.finditer(stripped):
            args = split_args(stripped, match.end() - 1)
            index = DISPLAY_CALLS[match.group(1)]
            if index >= len(args):
                continue
            argument = args[index]
            if re.search(r'\b(?:' + MACROS + r')', argument):
                continue
            runs = literals_at_top_level(argument)
            if not runs:
                continue
            literal = decode_cpp(" ".join(runs))
            if literal.startswith("##") or "\\0" in literal or len(literal) < 2:
                continue
            if not re.search(r"[A-Za-z]{3}", carrier_text(literal)):
                continue
            line = stripped.count("\n", 0, match.start()) + 1
            unwrapped.setdefault(relative, []).append((line, match.group(1), literal))

    findings = []
    for literal in sorted(used):
        if literal not in table:
            findings.append((f"{TABLE}", f"used but not translated: {literal!r}"))
    for source, context in sorted(contextual):
        if f"{source}|{context}" not in table:
            findings.append((TABLE, f"contextual entry missing: {source!r}|{context!r}"))
    for relative, items in sorted(unwrapped.items()):
        for line, function, literal in items:
            if relative in SKIP_FILES or literal in SKIP_LITERALS \
                    or f"{relative}|{literal}" in SKIP_LITERALS:
                continue
            findings.append((f"{relative}:{line}", f"{function}() draws an unwrapped literal: {literal!r}"))
    return findings, len(used)


def main():
    table, findings, markers = load_table()
    site_findings, used = check_call_sites(table)
    findings += site_findings
    findings += check_view_names(table)
    findings += check_node_tooltips(table)

    if "--quiet" not in sys.argv:
        for where, message in findings:
            print(f"{where}\n    {message}")
        if findings:
            print()

    sections = f" | {len(markers)} section markers" if markers else ""
    print(f"table {len(table)} entries{sections} | {used} distinct literals at call sites | "
          f"{len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
