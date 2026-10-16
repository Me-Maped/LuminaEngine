"""Format contracts mirrored in Localization/LocalizationFormat.cpp.

A table also contains prose and code. A valid source conversion (or escape) activates strict
parsing for that syntax. Literal markers alone do not activate it. New conversions are always
rejected; malformed markers are rejected when the source has no literal marker to explain them.
Brace contracts keep each field's spec and dynamic-argument roles, allowing indexed reordering.
Printf contracts retain conversion/length and ordered width/precision arguments, not fixed widths.
"""
import re

_PRINTF = re.compile(r"%([-+ #0']*)(\*|[0-9]*)(?:\.(\*|[0-9]*))?(hh|ll|[hljztL])?([diouxXfFeEgGaAcspn])")
_BRACE_START = re.compile(r"\{(?:\}|:|[0-9])")


def _printf(text, natural_space=False):
    args, escapes, valid, index = [], 0, True, 0
    while index < len(text):
        if text[index] != '%':
            index += 1
            continue
        if text.startswith('%%', index):
            escapes += 1
            index += 2
            continue
        match = _PRINTF.match(text, index)
        if match:
            flags, width, precision, length, kind = match.groups()
            length = length or ''
            # '% is' / '% complete' in prose is not a conversion followed by a word suffix.
            prose = ' ' in flags and ((match.end() < len(text) and text[match.end()].isascii()
                                      and text[match.end()].isalpha()) or natural_space)
            allowed = (length in ('', 'h', 'hh', 'l', 'll', 'j', 'z', 't') if kind in 'diouxXn'
                       else length in ('', 'l', 'L') if kind in 'fFeEgGaA'
                       else length in ('', 'l') if kind in 'cs' else length == '')
            if allowed and not prose:
                if width == '*': args.append('width:int')
                if precision == '*': args.append('precision:int')
                args.append(length + kind)
                index = match.end()
                continue
        valid = False
        index += 1
    return valid, tuple(args), escapes


def _braces(text):
    fields, escapes, index, automatic, mode = [], [0, 0], 0, 0, None

    def read_index():
        nonlocal index, automatic, mode
        start = index
        while index < len(text) and '0' <= text[index] <= '9': index += 1
        current_mode = 'manual' if index != start else 'auto'
        if mode is not None and mode != current_mode: raise ValueError()
        mode = current_mode
        if current_mode == 'manual':
            value = int(text[start:index])
            if value > 4294967295: raise ValueError()
            return value
        value = automatic
        automatic += 1
        return value

    def dynamic():
        nonlocal index
        index += 1
        value = read_index()
        if index >= len(text) or text[index] != '}': raise ValueError()
        index += 1
        return value

    try:
        while index < len(text):
            char = text[index]
            if char not in '{}':
                index += 1
                continue
            if text.startswith(char * 2, index):
                escapes[char == '}'] += 1
                index += 2
                continue
            if char == '}': raise ValueError()
            index += 1
            arg = read_index()
            spec, width, precision = '', -1, -1
            if index < len(text) and text[index] == ':':
                index += 1
                start = index
                if index + 1 < len(text) and text[index + 1] in '<>^' and text[index] not in '{}': index += 2
                elif index < len(text) and text[index] in '<>^': index += 1
                if index < len(text) and text[index] in '+- ': index += 1
                if index < len(text) and text[index] == '#': index += 1
                if index < len(text) and text[index] == '0': index += 1
                spec = text[start:index]
                if index < len(text) and text[index] == '{':
                    width = dynamic()
                    spec += '*'
                else:
                    start = index
                    while index < len(text) and '0' <= text[index] <= '9': index += 1
                    spec += text[start:index]
                if index < len(text) and text[index] == '.':
                    index += 1
                    spec += '.'
                    if index < len(text) and text[index] == '{':
                        precision = dynamic()
                        spec += '*'
                    else:
                        start = index
                        while index < len(text) and '0' <= text[index] <= '9': index += 1
                        if start == index: raise ValueError()
                        spec += text[start:index]
                if index < len(text) and text[index] == 'L':
                    spec += 'L'
                    index += 1
                if index < len(text) and text[index] != '}':
                    if text[index] not in 'sbcBdoxXaAeEfFgGpP': raise ValueError()
                    spec += text[index]
                    index += 1
            if index >= len(text) or text[index] != '}': raise ValueError()
            index += 1
            fields.append((arg, spec, width, precision))
        return True, tuple(sorted(fields)), tuple(escapes)
    except ValueError:
        return False, (), tuple(escapes)


def have_compatible_format_arguments(source, translation):
    """Source is the display text, with any table context suffix removed by the caller."""
    src = _printf(source)
    dst = _printf(translation, natural_space=not (src[1] or src[2]) and '%' in source)
    if src[1] or src[2]:
        if not src[0] or not dst[0] or src[1:] != dst[1:]: return False
    elif dst[1] or dst[2] or (not dst[0] and '%' not in source):
        return False
    src, dst = _braces(source), _braces(translation)
    active = bool(src[1] or any(src[2]) or _BRACE_START.search(source))
    if active:
        return src[0] and dst[0] and src[1:] == dst[1:]
    if dst[1] or any(dst[2]) or _BRACE_START.search(translation): return False
    return dst[0] or ('{' in source or '}' in source)
