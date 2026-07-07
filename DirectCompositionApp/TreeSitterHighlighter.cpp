#include "TreeSitterHighlighter.h"
#include <windows.h>
#include <tree_sitter/api.h>
#include <cstring>

extern "C" const TSLanguage* tree_sitter_cpp(void);

static const char* HIGHLIGHTS_QUERY =
    "; C highlights (inherited by C++)\n"
    "(identifier) @variable\n"
    "((identifier) @constant (#match? @constant \"^[A-Z][A-Z\\\\d_]*$\"))\n"
    "\"break\" @keyword\n"
    "\"case\" @keyword\n"
    "\"const\" @keyword\n"
    "\"continue\" @keyword\n"
    "\"default\" @keyword\n"
    "\"do\" @keyword\n"
    "\"else\" @keyword\n"
    "\"enum\" @keyword\n"
    "\"extern\" @keyword\n"
    "\"for\" @keyword\n"
    "\"if\" @keyword\n"
    "\"inline\" @keyword\n"
    "\"return\" @keyword\n"
    "\"sizeof\" @keyword\n"
    "\"static\" @keyword\n"
    "\"struct\" @keyword\n"
    "\"switch\" @keyword\n"
    "\"typedef\" @keyword\n"
    "\"union\" @keyword\n"
    "\"volatile\" @keyword\n"
    "\"while\" @keyword\n"
    "\"#define\" @keyword\n"
    "\"#elif\" @keyword\n"
    "\"#else\" @keyword\n"
    "\"#endif\" @keyword\n"
    "\"#if\" @keyword\n"
    "\"#ifdef\" @keyword\n"
    "\"#ifndef\" @keyword\n"
    "\"#include\" @keyword\n"
    "(preproc_directive) @keyword\n"
    "\"--\" @operator\n"
    "\"-\" @operator\n"
    "\"-=\" @operator\n"
    "\"->\" @operator\n"
    "\"=\" @operator\n"
    "\"!=\" @operator\n"
    "\"*\" @operator\n"
    "\"&\" @operator\n"
    "\"&&\" @operator\n"
    "\"+\" @operator\n"
    "\"++\" @operator\n"
    "\"+=\" @operator\n"
    "\"<\" @operator\n"
    "\"==\" @operator\n"
    "\">\" @operator\n"
    "\"||\" @operator\n"
    "\".\" @delimiter\n"
    "\";\" @delimiter\n"
    "(string_literal) @string\n"
    "(system_lib_string) @string\n"
    "(null) @constant\n"
    "(number_literal) @number\n"
    "(char_literal) @number\n"
    "(field_identifier) @property\n"
    "(statement_identifier) @label\n"
    "(type_identifier) @type\n"
    "(primitive_type) @type\n"
    "(sized_type_specifier) @type\n"
    "(call_expression function: (identifier) @function)\n"
    "(call_expression function: (field_expression field: (field_identifier) @function))\n"
    "(function_declarator declarator: (identifier) @function)\n"
    "(preproc_function_def name: (identifier) @function)\n"
    "(comment) @comment\n"
    "\n"
    "; C++ specific highlights\n"
    "(call_expression function: (qualified_identifier name: (identifier) @function))\n"
    "(template_function name: (identifier) @function)\n"
    "(template_method name: (field_identifier) @function)\n"
    "(function_declarator declarator: (qualified_identifier name: (identifier) @function))\n"
    "(function_declarator declarator: (field_identifier) @function)\n"
    "((namespace_identifier) @type (#match? @type \"^[A-Z]\"))\n"
    "(auto) @type\n"
    "(this) @variable.builtin\n"
    "(null \"nullptr\" @constant)\n"
    "(raw_string_literal) @string\n"
    "[\"catch\" \"class\" \"co_await\" \"co_return\" \"co_yield\" \"constexpr\" \"constinit\"\n"
    " \"consteval\" \"delete\" \"explicit\" \"final\" \"friend\" \"mutable\" \"namespace\"\n"
    " \"noexcept\" \"new\" \"override\" \"private\" \"protected\" \"public\" \"template\"\n"
    " \"throw\" \"try\" \"typename\" \"using\" \"concept\" \"requires\" \"virtual\"\n"
    " \"import\" \"export\" \"module\"] @keyword\n";

TreeSitterHighlighter::TreeSitterHighlighter() {}

TreeSitterHighlighter::~TreeSitterHighlighter() {
    if (m_cursor) ts_query_cursor_delete(m_cursor);
    if (m_query) ts_query_delete(m_query);
    if (m_tree) ts_tree_delete(m_tree);
    if (m_parser) ts_parser_delete(m_parser);
}

bool TreeSitterHighlighter::Initialize() {
    m_parser = ts_parser_new();
    if (!m_parser) return false;

    if (!ts_parser_set_language(m_parser, tree_sitter_cpp())) {
        OutputDebugStringA("[TreeSitter] Language version mismatch or load failed\n");
        return false;
    }

    uint32_t error_offset = 0;
    TSQueryError error_type = TSQueryErrorNone;
    m_query = ts_query_new(
        tree_sitter_cpp(),
        HIGHLIGHTS_QUERY,
        (uint32_t)strlen(HIGHLIGHTS_QUERY),
        &error_offset,
        &error_type
    );
    if (!m_query) {
        char buf[256];
        sprintf_s(buf, "[TreeSitter] Query error at offset %u, type %d\n", error_offset, error_type);
        OutputDebugStringA(buf);
        return false;
    }

    m_cursor = ts_query_cursor_new();
    return m_cursor != nullptr;
}

void TreeSitterHighlighter::UpdateSource(const char* source, uint32_t length) {
    if (!m_parser) return;
    if (m_tree) {
        ts_tree_delete(m_tree);
        m_tree = nullptr;
    }
    m_tree = ts_parser_parse_string(m_parser, nullptr, source, length);
    m_source.assign(source, length);
    RunQuery();
}

void TreeSitterHighlighter::RunQuery() {
    m_highlights.clear();
    if (!m_tree || !m_query || !m_cursor) return;

    TSNode root = ts_tree_root_node(m_tree);
    ts_query_cursor_exec(m_cursor, m_query, root);

    TSQueryMatch match;
    uint32_t captureIndex;
    while (ts_query_cursor_next_capture(m_cursor, &match, &captureIndex)) {
        const TSQueryCapture& capture = match.captures[captureIndex];
        uint32_t length = 0;
        const char* name = ts_query_capture_name_for_id(m_query, capture.index, &length);
        TSNode node = capture.node;

        uint32_t startByte = ts_node_start_byte(node);
        uint32_t endByte = ts_node_end_byte(node);

        if (startByte < endByte && endByte <= (uint32_t)m_source.size()) {
            m_highlights.push_back({ startByte, endByte, name });
        }
    }
}
