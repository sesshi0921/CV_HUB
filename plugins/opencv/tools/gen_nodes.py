#!/usr/bin/env python3
"""
Build-time generator: parses CV_EXPORTS_W functions from OpenCV headers
and emits C++ registration code for cvhub.

Usage: gen_opencv_nodes.py --headers <h1> [<h2>...] --output <out.cpp>
"""

import re
import argparse
import sys
from pathlib import Path


# ─── type classification ─────────────────────────────────────────────────────

INPUT_TYPES  = {'InputArray', 'InputArrayOfArrays'}
OUTPUT_TYPES = {'OutputArray', 'OutputArrayOfArrays'}

# Maps C++ base type → (SemanticType, cpp_reader, default_extractor)
SCALAR_MAP = {
    'int':    'Integer',
    'double': 'Float',
    'float':  'Float',
    'bool':   'Boolean',
    'Size':   'Integer',  # exposed as single int → cv::Size(k, k)
}


def base_type(type_str: str) -> str:
    """Strip const / ref / cv:: / qualifiers."""
    t = re.sub(r'\b(const|CV_IN_OUT|CV_OUT|CV_IN)\b', '', type_str)
    t = re.sub(r'[&*]', '', t)
    t = re.sub(r'\bcv::', '', t)
    return t.strip()


def camel_to_snake(name: str) -> str:
    s = re.sub(r'(?<=[a-z0-9])([A-Z])', r'_\1', name)
    return s.lower()


# ─── header parsing ───────────────────────────────────────────────────────────

def split_params(params_str: str) -> list[str]:
    """Split on commas at depth 0 (respects <> and ())."""
    parts, depth, curr = [], 0, ''
    for ch in params_str:
        if ch in '<(':
            depth += 1
            curr += ch
        elif ch in '>)':
            depth -= 1
            curr += ch
        elif ch == ',' and depth == 0:
            parts.append(curr.strip())
            curr = ''
        else:
            curr += ch
    if curr.strip():
        parts.append(curr.strip())
    return parts


def parse_param(p: str):
    """Return (type_str, name, default_str | None) or None."""
    p = re.sub(r'\s+', ' ', p).strip()
    m = re.match(r'^(.*?)\s+(\w+)\s*(?:=\s*(.+))?$', p)
    if not m:
        return None
    return m.group(1).strip(), m.group(2).strip(), (m.group(3).strip() if m.group(3) else None)


def parse_header(path: str) -> list[dict]:
    """Extract all CV_EXPORTS_W functions that have (InputArray src, OutputArray dst) pattern."""
    content = Path(path).read_text(encoding='utf-8', errors='ignore')
    # Strip line-comments to avoid confusing the regex
    content = re.sub(r'//[^\n]*', '', content)
    # Match: CV_EXPORTS_W <ret> <name>(<params>);
    pattern = re.compile(
        r'CV_EXPORTS_W\s+(?:void|(?:\w[\w:]*?))\s+(\w+)\s*\(([^;]+?)\)\s*;',
        re.DOTALL
    )
    results = []
    for m in pattern.finditer(content):
        func_name = m.group(1)
        params_raw = re.sub(r'\s+', ' ', m.group(2))
        raw_params = split_params(params_raw)
        params = [parse_param(p) for p in raw_params]
        params = [p for p in params if p is not None]
        if not params:
            continue
        # Needs both input and output image ports
        has_in  = any(base_type(t) in INPUT_TYPES  for t, _, _ in params)
        has_out = any(base_type(t) in OUTPUT_TYPES for t, _, _ in params)
        if not has_in or not has_out:
            continue
        results.append({'name': func_name, 'params': params})
    return results


# ─── per-function code generation ────────────────────────────────────────────

def build_function(func: dict):
    """
    Returns (lambda_lines, descriptor_args, call_args) or None if unsupported.
    lambda_lines  : list of C++ statements for the lambda body (after src decl)
    descriptor_args: list of ArgumentDescriptor snippets for params
    call_args     : ordered list of args to pass to cv::FuncName(...)
    """
    params = func['params']
    lambda_decls = []
    call_args    = []
    desc_args    = []

    for type_str, name, default in params:
        bt = base_type(type_str)

        if bt in INPUT_TYPES:
            call_args.append('src')
            continue

        if bt in OUTPUT_TYPES:
            call_args.append('dst')
            continue

        # ── int ─────────────────────────────────────────────────────────────
        if bt == 'int':
            dv = 0
            if default:
                m = re.search(r'-?\d+', default)
                if m:
                    try:
                        dv = int(m.group(0))
                    except ValueError:
                        pass
            lambda_decls.append(
                f'        const int {name} = parameterAs<int>(p, "{name}", {dv});'
            )
            call_args.append(name)
            desc_args.append(
                f'{{.name="{name}",.displayName="{name}",'
                f'.semanticType=SemanticType::Integer,'
                f'.direction=ArgumentDirection::Parameter,.defaultValue=(int){dv}}}'
            )
            continue

        # ── double / float ───────────────────────────────────────────────────
        if bt in ('double', 'float'):
            dv = 0.0
            if default:
                m = re.search(r'-?[\d.]+', default)
                if m:
                    try:
                        dv = float(m.group(0))
                    except ValueError:
                        pass
            if bt == 'double':
                lambda_decls.append(
                    f'        const double {name} = parameterAs<double>(p, "{name}", {dv:.6g});'
                )
            else:
                lambda_decls.append(
                    f'        const float {name} = static_cast<float>(parameterAs<double>(p, "{name}", {dv:.6g}));'
                )
            call_args.append(name)
            desc_args.append(
                f'{{.name="{name}",.displayName="{name}",'
                f'.semanticType=SemanticType::Float,'
                f'.direction=ArgumentDirection::Parameter,.defaultValue=(double){dv:.6g}}}'
            )
            continue

        # ── bool ─────────────────────────────────────────────────────────────
        if bt == 'bool':
            dv_cpp = 'true' if default and 'true' in default.lower() else 'false'
            lambda_decls.append(
                f'        const bool {name} = parameterAs<bool>(p, "{name}", {dv_cpp});'
            )
            call_args.append(name)
            desc_args.append(
                f'{{.name="{name}",.displayName="{name}",'
                f'.semanticType=SemanticType::Boolean,'
                f'.direction=ArgumentDirection::Parameter,.defaultValue=(bool){dv_cpp}}}'
            )
            continue

        # ── Size (expose as square-kernel int) ───────────────────────────────
        if bt == 'Size':
            lambda_decls.append(
                f'        const int {name}_k = parameterAs<int>(p, "{name}", 3);'
            )
            lambda_decls.append(
                f'        const cv::Size {name}({name}_k, {name}_k);'
            )
            call_args.append(name)
            desc_args.append(
                f'{{.name="{name}",.displayName="{name}",'
                f'.semanticType=SemanticType::Integer,'
                f'.direction=ArgumentDirection::Parameter,.defaultValue=(int)3}}'
            )
            continue

        # ── complex type with default — omit trailing args ───────────────────
        if default is not None:
            break  # all remaining args have defaults; omit from call

        # ── required complex type — skip this function ────────────────────────
        return None

    return lambda_decls, desc_args, call_args


# ─── full file generation ─────────────────────────────────────────────────────

HEADER = """\
// AUTO-GENERATED by tools/gen_opencv_nodes.py — do not edit manually.
// Re-generated by CMake whenever the generator script changes.

#include "opencv_helpers.hpp"
#include "cvhub/runtime/generic_node.hpp"
#include "cvhub/runtime/function_descriptor.hpp"
#include "cvhub/runtime/service_container.hpp"
#include "cvhub/core/types.hpp"

{includes}

namespace cvhub::plugins::opencv {{

"""

FOOTER = """\
} // namespace cvhub::plugins::opencv
"""


def generate(functions: list[dict], header_includes: list[str]) -> str:
    includes = '\n'.join(f'#include <{h}>' for h in header_includes)

    desc_entries   = []
    factory_entries = []

    for func in functions:
        result = build_function(func)
        if result is None:
            continue
        lambda_decls, desc_args, call_args = result
        name  = func['name']
        snake = camel_to_snake(name)
        key   = f'opencv.{snake}'

        # ── FunctionDescriptor entry ─────────────────────────────────────────
        desc_param_args = '\n        '.join(
            f'        {a},' for a in desc_args
        )
        desc_entries.append(f"""\
    FunctionDescriptor{{
      .library     = "opencv",
      .namespaceName = "cv",
      .functionName  = "{snake}",
      .qualifiedName = "cv::{name}",
      .displayName   = "cv::{name}",
      .category      = "filter",
      .arguments = {{
        {{.name="image",.displayName="Image",.semanticType=SemanticType::Image,.direction=ArgumentDirection::Input}},
        {{.name="image",.displayName="Image",.semanticType=SemanticType::Image,.direction=ArgumentDirection::Output}},
        {desc_param_args}
      }},
      .factoryKey    = "{key}",
      .explicitKind  = NodeKind::Transform,
    }},""")

        # ── factory entry ────────────────────────────────────────────────────
        body_lines = '\n'.join(lambda_decls)
        args_str   = ', '.join(call_args)
        factory_entries.append(f"""\
    {{"{key}", [](NodeDescriptor d) {{
      return std::make_shared<GenericImageTransformFactory>(
        std::move(d),
        [](std::shared_ptr<const ImageValue> input, const ParameterMap& p)
            -> std::shared_ptr<ImageValue> {{
          const cv::Mat src = imageToMat(*input);
          const PixelFormat fmt = input->pixelFormat();
{body_lines}
          cv::Mat dst;
          cv::{name}({args_str});
          return matToImage(dst, fmt);
        }}
      );
    }}}},""")

    desc_block    = '\n'.join(desc_entries)
    factory_block = '\n'.join(factory_entries)

    return (
        HEADER.format(includes=includes)
        + "std::vector<FunctionDescriptor> getAutoOpenCVDescriptors() {\n"
        + f"  return {{\n{desc_block}\n  }};\n}}\n\n"
        + "std::unordered_map<std::string, FactoryFn> getAutoOpenCVRegistry() {\n"
        + f"  return {{\n{factory_block}\n  }};\n}}\n\n"
        + FOOTER
    )


# ─── main ─────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--headers', nargs='+', required=True,
                    help='OpenCV header paths to parse')
    ap.add_argument('--includes', nargs='*', default=[],
                    help='#include <...> names to emit in generated file')
    ap.add_argument('--output', required=True,
                    help='Output .cpp path')
    args = ap.parse_args()

    all_funcs: dict[str, dict] = {}
    for hpath in args.headers:
        for func in parse_header(hpath):
            # deduplicate by function name (first occurrence wins)
            all_funcs.setdefault(func['name'], func)

    # Default includes: derive from header paths
    includes = args.includes or [
        Path(h).name for h in args.headers
        if Path(h).name.endswith('.hpp')
    ]

    cpp = generate(list(all_funcs.values()), includes)
    Path(args.output).write_text(cpp, encoding='utf-8')
    print(f'[gen_opencv_nodes] generated {len(all_funcs)} candidates → {args.output}',
          file=sys.stderr)


if __name__ == '__main__':
    main()
