"""Keeps the flat string table in a fixed module order, and can index it for a human.

    python BuildScripts/LocalizationTable.py --stats              per-block counts
    python BuildScripts/LocalizationTable.py --reorder            rewrite the table in that order
    python BuildScripts/LocalizationTable.py --reorder --out P    write to P instead, for a preview
    python BuildScripts/LocalizationTable.py --check              verify the table is already canonical
    python BuildScripts/LocalizationTable.py --outline [P]        per-block index; stdout when no P
    python BuildScripts/LocalizationTable.py --dump-view-names    regenerate the derived name lists

The table is one flat object keyed by the English source, which is the invariant every LOC macro
depends on, so entries cannot carry a module tag without breaking the lookup. What a reader needs
is not a tag but for related strings to be near each other, and that is a property of the file's
*order* -- a flat object preserves it, a hash map does not care. So this script owns one canonical
order: a fixed list of blocks, alphabetical inside each block, and a marker between blocks.

    "__section 20-content-browser__": { "title": "内容浏览器", "count": 124 },

The marker's value is an object rather than a string because FLocalization::ParseTable skips
non-string values on purpose (see its comment, and the Localization.NonStringValuesAreSkipped test
that pins it), so markers load as nothing at all -- there is no side table to keep in sync and no
runtime change to make. It costs two things: any tool that walks
the table has to skip non-string values too, and the file's order is now semantic, so it has to be
verifiable. `--check` is that verification, and it is what makes the order survive contact with
editors.

WHERE A STRING COMES FROM decides its block, and the three sources need different rules:

  * A literal handed to LOC/LOC_ICON/LOC_CTX has a call site, and the call site's directory names
    the module. A string two modules both use goes to Z5-shared rather than being duplicated --
    in a flat table a shared string is a single entry either way, which is the point.

  * A property display name or a property category segment has no call site at all: the details
    panel derives the key at runtime from the reflected member name, and the reflection metadata
    carries the category tree. Those come from the build's generated reflection, dumped into
    LocalizationData/view-names.json so that ordering the table does not require a build.

  * A contextual key ("Source|domain") is not a string in its own right, so no rule above can see
    it; it is placed with the Source it disambiguates, next to which a reader expects to find it.

  * Everything else is reached as a runtime view (a pin name, a node name, a window title), which
    no call-site scan can see. Those are classified by the statement the literal sits in, which is
    how a pin ("CreateInputPin(\"Out Left\", ...)") is told from a window title ("...Name = \"UVs\"").
    A literal the rules cannot place lands in ZB-help-and-misc, which is a legitimate answer rather
    than a failure: the block is still stable, sorted, and findable.

  * A node tooltip is reached the same way, but cannot be classified that way: it is long prose, so
    most are a multi-line concatenation whose first line reads only "return", and the statement a
    literal sits in says nothing about where it is drawn. Its declaration does, so those are read
    from GetNodeTooltip itself -- the same reader the checker uses to require an entry, which is
    what keeps the two from drifting apart.

Blocks are ordered the way the editor is used, so the file reads top-down the way the UI is
navigated, and the two derived blocks sit after the shipped code so that adding an editor string
never shifts them.
"""

import argparse
import collections
import io
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import CheckLocalization as check  # noqa: E402  (the shared C++ literal scanner)

ROOT = check.ROOT
TABLE = "Engine/Resources/Localization/zh-Hans.json"
VIEW_DATA = "BuildScripts/LocalizationData/view-names.json"

# Where --dump-view-names reads the reflection tool's output. The hash directory is derived from
# the checkout path, so it is globbed rather than named.
REFLECTION_ROOT = "Intermediates/Reflection"

# The canonical block order. The id is the marker's key and the outline's anchor; the title is what
# a reader sees. Ids sort in this order on their own, so a stray tool that sorts keys keeps the
# blocks together.
BLOCKS = [
    ("00-editor-chrome", "主菜单与编辑器外壳", "主菜单栏、Dock、设置、通知、进度、工具框架、工厂对话框"),
    ("10-content-browser", "内容浏览器", ""),
    ("20-world-editor", "世界编辑器", "视口、大纲、地形、序列器、替换引用、组件可视化"),
    ("30-properties", "细节面板绘制", "属性行、工具提示、过滤、定制器画出来的字"),
    ("40-node-graphs", "节点图", "材质、动画、音频，以及通用节点图外壳"),
    ("50-asset-editors", "资产编辑器", "材质、纹理、网格、动画、数据表、预制体等"),
    ("60-import", "导入器", ""),
    ("70-debug-tools", "调试与诊断工具", "性能分析、控制台、资源注册表、对象浏览器、打包"),
    ("80-plugins", "插件", "网络编辑器等"),
    ("Z0-runtime", "运行时与游戏侧", "引擎运行时模块里画出来的字"),
    ("Z5-shared", "跨模块共用", "被两个以上源码块用到的来源"),
    ("Z6-property-names", "属性名与分组名", "反射推导出的显示名，以及分类树的每一段"),
    ("Z7-pin-names", "引脚与端口名", "节点图端口的名字"),
    ("Z8-window-names", "窗口、面板与菜单区标题", ""),
    ("Z9-display-names", "类型覆写出来的显示名", "GetDisplayName/GetAssetName/GetNodeCategory 返回的名字"),
    ("ZA-enum-labels", "枚举与阶段标签", "枚举转显示名的分支和查表"),
    ("ZB-help-and-misc", "帮助、示例与其余", "画出来的说明文字、代码样例、插件文本"),
    ("ZC-node-tooltips", "节点悬停说明", "GetNodeTooltip 返回的整段说明文字"),
]

BLOCK_IDS = [block[0] for block in BLOCKS]
BLOCK_TITLES = {block[0]: block[1] for block in BLOCKS}
BLOCK_NOTES = {block[0]: block[2] for block in BLOCKS}

# ---------------------------------------------------------------------------------------------
# The call-site rule: a source directory maps to a block.
# ---------------------------------------------------------------------------------------------

# Ordered most specific first; the first match wins. A file that matches nothing is editor chrome,
# which is where the small generic pieces (widgets, modals, factories) live.
def block_for_source(relative):
    name = relative.split("/")[-1]

    if relative.startswith("Engine/Plugins/"):
        return "80-plugins"
    if relative.startswith("Engine/Editor/Source/UI/Tools/NodeGraph/"):
        return "40-node-graphs"
    if relative.startswith("Engine/Editor/Source/UI/Tools/AssetEditors/"):
        return "50-asset-editors"
    if relative.startswith("Engine/Editor/Source/UI/Tools/Debug/"):
        return "70-debug-tools"
    if relative.startswith("Engine/Editor/Source/Tools/Import/") or name.startswith("Importer"):
        return "60-import"
    if relative.startswith("Engine/Editor/Source/Tools/ComponentVisualizers/"):
        return "20-world-editor"

    if "/Profiler" in relative:
        return "70-debug-tools"

    if name in ("WorldEditorTool.cpp", "FSceneEditorTool.cpp", "TerrainEditMode.cpp",
                "SequencerEditMode.cpp", "ReplaceReferencesModal.cpp", "ModelingEditMode.cpp",
                "FoliageEditMode.cpp", "NavMeshEditMode.cpp", "GamePreviewTool.cpp",
                "EditorTool.cpp", "EditorAssetActions.cpp", "EntityHandleCustomization.cpp"):
        return "20-world-editor"
    if "ContentBrowser" in name:
        return "10-content-browser"
    if name == "EditorUI.cpp":
        return "00-editor-chrome"

    if relative.startswith("Engine/Editor/Source/UI/Properties/"):
        return "30-properties"
    if relative.startswith("Engine/Editor/Source/UI/CurveEditor/"):
        return "30-properties"

    # Everything left under UI/Tools is a tool window of some kind. The ones with a block of their
    # own are matched above; the rest (console, plugin browser, terminal, shadow atlas, ...) sit
    # with the diagnostics they resemble.
    if relative.startswith("Engine/Editor/Source/UI/Tools/"):
        return "70-debug-tools"

    if relative.startswith("Engine/Source/Runtime/"):
        return "Z0-runtime"

    return "00-editor-chrome"


# ---------------------------------------------------------------------------------------------
# The view rule: the statement a literal sits in. Only reached for keys with no call site and no
# reflection owner, so it has to be a heuristic -- each pattern is a shape seen in the tree, and
# the fallback block is a stated answer rather than a hole.
# ---------------------------------------------------------------------------------------------

VIEW_RULES = [
    # The order is the precedence, and it runs from the most specific shape to the least: a pin
    # name is declared by a call that says Pin, so it can be told apart without guessing.
    #
    # CreateInputPin("Out Left", ...), CreateAnimPin("A", ...), AddPin(...), PinName = "..."
    ("Z7-pin-names", re.compile(r"Create\w*Pin|AddPin|PinName|PinLabel|GetPinName|SGraphPin|EPinDirection")),
    # static const char* GraphName = "Material Graph"; CreateToolWindow("Console", ...);
    ("Z8-window-names", re.compile(r"\b\w*Name\s*=|CreateToolWindow|FEditorTool\(|WindowName|DrawMenuSection|GetToolName")),
    # FString GetDisplayName() const override { return "Curl Noise Force"; } -- also GetAssetName,
    # which is the display name of an asset type, and GetNodeCategory, which titles a node.
    ("Z9-display-names", re.compile(r"GetDisplayName\(\)|GetAssetName\(\)|GetNodeCategory\(\)|GetNodeDisplayName\(\)")),
    # case EPluginLoadingPhase::Earliest: return "Earliest"; and { EType::Value, "Label" } tables.
    ("ZA-enum-labels", re.compile(r"\bcase\s+\w+::|\{\s*E\w+::")),
]


def classify_view(sites):
    """Block for a view-reached key, from the statements it appears in."""
    tags = set()
    for _, text in sites:
        tags.add(heading_for(text))
    for block, _pattern in VIEW_RULES:
        if block in tags:
            return block
    return "ZB-help-and-misc"


def heading_for(text):
    for block, pattern in VIEW_RULES:
        if pattern.search(text):
            return block
    return "ZB-help-and-misc"


# ---------------------------------------------------------------------------------------------

LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def read_table():
    """The table's entries, in file order, with the section markers pulled out separately."""
    with io.open(os.path.join(ROOT, TABLE), "rb") as handle:
        raw = handle.read()
    if raw.startswith(b"\xef\xbb\xbf"):
        raise SystemExit(f"{TABLE}: has a UTF-8 BOM; the table is a plain UTF-8 document")
    text = raw.decode("utf-8")
    if text.endswith("\n"):
        raise SystemExit(f"{TABLE}: ends with a newline; the table is written without one")

    try:
        parsed = json.loads(text, object_pairs_hook=collections.OrderedDict)
    except ValueError as error:
        raise SystemExit(f"{TABLE}: not valid JSON ({error})")

    entries, markers = collections.OrderedDict(), []
    for key, value in parsed.items():
        if isinstance(value, str):
            entries[key] = value
        else:
            markers.append(key)
    return entries, markers


def load_view_data():
    path = os.path.join(ROOT, VIEW_DATA)
    if not os.path.exists(path):
        raise SystemExit(f"missing {VIEW_DATA}; run --dump-view-names after a build")
    with io.open(path, encoding="utf-8") as handle:
        data = json.load(handle)
    return set(data["property_display_names"]), set(data["property_category_segments"])


def line_text(text, offset):
    start = text.rfind("\n", 0, offset) + 1
    end = text.find("\n", offset)
    return text[start:end if end != -1 else len(text)].strip()


def scan_source(table_keys):
    """One pass over the source: which files call LOC on a key, where each key also appears raw, and
    which keys are a node tooltip.

    All three are needed. A call site says which module owns a string; a raw occurrence is
    the only clue for a string reached as a runtime view, since by definition nobody passes it to
    a macro; and a tooltip is drawn through one of those runtime views too, but its shape is prose
    rather than a name, so it is recognised by the declaration that returns it.
    """
    call_sites = collections.defaultdict(set)
    raw_sites = collections.defaultdict(list)
    tooltips = set()

    for full, relative in check.source_files():
        try:
            with io.open(full, encoding="utf-8-sig") as handle:
                text = handle.read()
        except (UnicodeDecodeError, OSError):
            continue
        stripped = check.strip_comments(text)

        for literal in check.localization_literals(stripped):
            call_sites[literal].add(relative)
        for match in check.CTX_CALL.finditer(stripped):
            call_sites[f"{check.decode_cpp(match.group(1))}|{match.group(2)}"].add(relative)

        for _offset, literal in check.node_tooltips(stripped):
            if literal:
                tooltips.add(literal)

        for match in LITERAL.finditer(stripped):
            try:
                literal = check.decode_cpp(match.group(0))
            except Exception:
                continue
            if literal in table_keys:
                raw_sites[literal].append((relative, line_text(stripped, match.start())))

    return call_sites, raw_sites, tooltips


def assign(entries, view_data):
    """key -> block, for every key in the table."""
    call_sites, raw_sites, tooltips = scan_source(set(entries))
    names, categories = view_data

    blocks, from_call, from_reflection, from_tooltip = {}, 0, 0, 0
    contextual = []
    for key in entries:
        # "Source|domain" is a disambiguated form of Source, so it belongs beside it: that is where
        # a reader looks for it. Parked for a second pass because Source's own block has to be
        # decided first -- and a sentence that merely contains a pipe is left to the rules below,
        # which is what makes this check "the prefix is itself a key" rather than "contains a pipe".
        if "|" in key and key.split("|", 1)[0] in entries:
            contextual.append(key)
            continue

        sites = call_sites.get(key)
        if sites:
            modules = {block_for_source(relative) for relative in sites}
            blocks[key] = modules.pop() if len(modules) == 1 else "Z5-shared"
            from_call += 1
        elif key in names or key in categories:
            blocks[key] = "Z6-property-names"
            from_reflection += 1
        elif key in tooltips:
            blocks[key] = "ZC-node-tooltips"
            from_tooltip += 1
        else:
            blocks[key] = classify_view(raw_sites.get(key, []))

    for key in contextual:
        blocks[key] = blocks.get(key.split("|", 1)[0], "ZB-help-and-misc")

    return blocks, from_call, from_reflection, from_tooltip, call_sites


# ---------------------------------------------------------------------------------------------
# The record layout. One entry per line, which is what a diff of a reorder should look like.
# ---------------------------------------------------------------------------------------------

def encode(value):
    return json.dumps(value, ensure_ascii=False)


def order_keys(keys):
    """Alphabetical, but on the text a reader sees: several keys are indented help or sample
    lines, and sorting them by their raw bytes would open every block with a run of blanks."""
    return sorted(keys, key=lambda key: (key.lstrip(), key))


def render(entries, blocks):
    """The table as text, in the canonical order."""
    grouped = collections.defaultdict(list)
    for key, block in blocks.items():
        if block not in BLOCK_TITLES:
            raise SystemExit(f"{key!r} was placed in unknown block {block!r}")
        grouped[block].append(key)

    lines = ["{"]
    for block in BLOCK_IDS:
        keys = order_keys(grouped.get(block, []))
        if not keys:
            continue
        title = BLOCK_TITLES[block]
        lines.append(f"    {encode(f'__section {block}__')}: "
                     f"{{ \"title\": {encode(title)}, \"count\": {len(keys)} }},")
        for key in keys:
            lines.append(f"    {encode(key)}: {encode(entries[key])},")

    # The last entry keeps no trailing comma, so the closing brace stays next to it the way the
    # file has always been written.
    lines[-1] = lines[-1].rstrip(",")
    lines.append("}")
    return "\n".join(lines)


MARKER = re.compile(r'^\s*"__section ([\w-]+)__":')
ENTRY = re.compile(r'^\s*"((?:[^"\\]|\\.)*)": ')


def parse_file(text):
    """The file's markers in order, and which block each entry actually sits in."""
    markers, placement = [], {}
    block = None
    for line in text.split("\n"):
        marker = MARKER.match(line)
        if marker:
            block = marker.group(1)
            markers.append(block)
            continue
        entry = ENTRY.match(line)
        if entry and block is not None:
            placement[json.loads('"' + entry.group(1) + '"')] = block
    return markers, placement


def marker_counts(text):
    counts = {}
    for match in re.finditer(r'^\s*"__section ([\w-]+)__": \{ "title": "(?:[^"\\]|\\.)*", "count": (\d+) \},',
                             text, re.M):
        counts[match.group(1)] = int(match.group(2))
    return counts


def check_order(entries, blocks):
    """Compares the file on disk against the canonical order."""
    with io.open(os.path.join(ROOT, TABLE), encoding="utf-8") as handle:
        current = handle.read()

    if current == render(entries, blocks):
        return []

    findings = []
    markers, placement = parse_file(current)
    expected = [block for block in BLOCK_IDS if any(blocks.get(key) == block for key in entries)]

    if markers != expected:
        findings.append(f"blocks are {markers}, expected {expected}")

    counts, wanted = marker_counts(current), collections.Counter(blocks.values())
    for block in expected:
        if block in counts and counts[block] != wanted[block]:
            findings.append(f"__section {block}__ says {counts[block]} entries, "
                            f"but {wanted[block]} belong there")

    # A new entry dropped into the wrong block leaves the markers and the counts right, so each
    # entry's home is checked as well. This is the failure a contributor actually hits.
    misplaced = [(key, placement[key], blocks[key]) for key in entries
                 if key in placement and placement[key] != blocks[key]]
    for key, actual, correct in misplaced[:5]:
        findings.append(f"{key!r} sits in {actual}, belongs in {correct}")
    if len(misplaced) > 5:
        findings.append(f"... and {len(misplaced) - 5} more entries in the wrong block")

    if not findings:
        current_lines, expected_lines = current.split("\n"), render(entries, blocks).split("\n")
        for index, pair in enumerate(zip(current_lines, expected_lines)):
            if pair[0] != pair[1]:
                findings.append(f"line {index + 1} is not in canonical order: "
                                f"on disk {pair[0].strip()[:60]!r}, expected {pair[1].strip()[:60]!r}")
                break
        else:
            findings.append(f"file has {len(current_lines)} lines, expected {len(expected_lines)}")
    return findings


def outline(entries, blocks):
    text = io.open(os.path.join(ROOT, TABLE), encoding="utf-8").read().split("\n")
    line_of = {}
    for index, line in enumerate(text):
        match = re.match(r'    "((?:[^"\\]|\\.)*)": ', line)
        if match:
            line_of[json.loads('"' + match.group(1) + '"')] = index + 1

    grouped = collections.defaultdict(list)
    for key, block in blocks.items():
        grouped[block].append(key)

    out = ["# zh-Hans 词条表 · 按模块索引",
           "",
           "生成物，不要手改：`python BuildScripts/LocalizationTable.py --outline`。",
           "行号指向 `Engine/Resources/Localization/zh-Hans.json`。",
           "",
           "| 块 | 标题 | 条数 | 行号 |",
           "| --- | --- | ---: | --- |"]
    for block in BLOCK_IDS:
        keys = order_keys(grouped.get(block, []))
        if not keys:
            continue
        spans = [line_of[key] for key in keys if key in line_of]
        where = f"{min(spans)}–{max(spans)}" if spans else ""
        out.append(f"| `{block}` | {BLOCK_TITLES[block]} | {len(keys)} | {where} |")

    for block in BLOCK_IDS:
        keys = order_keys(grouped.get(block, []))
        if not keys:
            continue
        note = f" · {BLOCK_NOTES[block]}" if BLOCK_NOTES[block] else ""
        out += ["", f"## {block} — {BLOCK_TITLES[block]} （{len(keys)} 条）{note}", ""]
        for key in keys:
            out.append(f"- `{key}` → {entries[key]}")
    return "\n".join(out) + "\n"


def dump_view_names():
    """Regenerates the derived name lists from the reflection tool's generated output."""
    root = os.path.join(ROOT, REFLECTION_ROOT)
    generated = []
    for dirpath, _, names in os.walk(root):
        generated += [os.path.join(dirpath, name) for name in names
                      if name.endswith(".generated.cpp")]
    if not generated:
        raise SystemExit(f"no generated reflection under {REFLECTION_ROOT}; build first")

    param = re.compile(r'Construct_CStruct_Lumina_\w+_Statics::(\w+) = \{ '
                       r'"((?:[^"\\]|\\.)*)", (.*?), METADATA_PARAMS\(', re.M)
    meta = re.compile(r'static constexpr Lumina::FMetaDataPairParam (\w+)_Metadata\[\] = \{(.*?)\n\t\};', re.S)
    pair = re.compile(r'\{ "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)" \}')
    typeflags = re.compile(r'Lumina::EPropertyTypeFlags::(\w+)')

    display = set()
    categories = set()
    for path in generated:
        text = io.open(path, encoding="utf-8-sig").read()
        for match in param.finditer(text):
            display.add(display_name(typeflags.search(match.group(3)), match.group(1)))
        for match in meta.finditer(text):
            for name, value in pair.findall(match.group(2)):
                if name == "Category" and value:
                    categories.update(value.split("|"))

    payload = {"property_display_names": sorted(display),
               "property_category_segments": sorted(categories)}
    target = os.path.join(ROOT, VIEW_DATA)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    io.open(target, "w", encoding="utf-8", newline="\n").write(
        json.dumps(payload, ensure_ascii=False, indent=4) + "\n")
    print(f"{VIEW_DATA}: {len(display)} display names, {len(categories)} category segments")


def display_name(typeflags, member):
    """FProperty::MakeDisplayNameFromName, so the key here is the key the details panel looks up."""
    raw = member
    if typeflags is not None and typeflags.group(1) == "Bool" and len(raw) > 1 \
            and raw[0] == "b" and raw[1].isupper():
        raw = raw[1:]
    out = []
    for index, char in enumerate(raw):
        if index > 0 and char.isupper() and raw[index - 1] not in " _" and not raw[index - 1].isupper():
            out.append(" ")
        out.append(char)
    if out:
        out[0] = out[0].upper()
    return "".join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--stats", action="store_true", help="print per-block counts")
    parser.add_argument("--reorder", action="store_true", help="rewrite the table in canonical order")
    parser.add_argument("--check", action="store_true", help="verify the table is already canonical")
    parser.add_argument("--outline", nargs="?", const="-", metavar="PATH",
                        help="write a per-block index; stdout when no path is given")
    parser.add_argument("--dump-view-names", action="store_true",
                        help="regenerate LocalizationData/view-names.json from the reflection output")
    parser.add_argument("--out", metavar="PATH", help="where --reorder writes instead of the table")
    parser.add_argument("--table", metavar="PATH",
                        help="read this table instead of the shipped one, to review a preview")
    args = parser.parse_args()

    global TABLE
    if args.table:
        TABLE = args.table

    if args.dump_view_names:
        dump_view_names()
        return 0

    entries, _markers = read_table()
    blocks, from_call, from_reflection, from_tooltip, call_sites = assign(entries, load_view_data())

    if args.stats:
        sizes = collections.Counter(blocks.values())
        print(f"table          {len(entries):5d} entries")
        print(f"  from a call site      {from_call:5d}")
        print(f"  from reflection       {from_reflection:5d}")
        print(f"  from a node tooltip   {from_tooltip:5d}")
        print(f"  from a view heuristic {len(entries) - from_call - from_reflection - from_tooltip:5d}")
        print()
        print(f"{'block':22s} {'entries':>8s}  title")
        for block in BLOCK_IDS:
            if not sizes[block]:
                continue
            print(f"{block:22s} {sizes[block]:8d}  {BLOCK_TITLES[block]}")
        unassigned = [key for key in entries if key not in blocks]
        if unassigned:
            print(f"\nNOT ASSIGNED: {len(unassigned)} -> {unassigned[:5]}")
            return 1
        unknown = {block for block in blocks.values() if block not in BLOCK_TITLES}
        if unknown:
            print(f"\nUNKNOWN BLOCK: {sorted(unknown)}")
            return 1
        return 0

    if args.reorder:
        text = render(entries, blocks)
        target = args.out or os.path.join(ROOT, TABLE)
        io.open(target, "w", encoding="utf-8", newline="\n").write(text)
        print(f"wrote {target} ({len(entries)} entries in "
              f"{len({b for b in blocks.values()})} blocks)")
        return 0

    if args.check:
        findings = check_order(entries, blocks)
        for finding in findings:
            print(f"  {finding}")
        print(f"{TABLE}: {len(entries)} entries, {len(findings)} finding(s)")
        return 1 if findings else 0

    if args.outline is not None:
        text = outline(entries, blocks)
        if args.outline == "-":
            sys.stdout.write(text)
        else:
            io.open(args.outline, "w", encoding="utf-8", newline="\n").write(text)
            print(f"wrote {args.outline}")
        return 0

    parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
