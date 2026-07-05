#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct TSParser;
struct TSTree;
struct TSQuery;
struct TSQueryCursor;

struct HighlightRange {
    uint32_t startByte;
    uint32_t endByte;
    const char* captureName;
};

class TreeSitterHighlighter {
public:
    TreeSitterHighlighter();
    ~TreeSitterHighlighter();

    bool Initialize();
    void UpdateSource(const char* source, uint32_t length);
    const std::vector<HighlightRange>& GetHighlights() const { return m_highlights; }

private:
    void RunQuery();

    TSParser* m_parser = nullptr;
    TSTree* m_tree = nullptr;
    TSQuery* m_query = nullptr;
    TSQueryCursor* m_cursor = nullptr;
    std::vector<HighlightRange> m_highlights;
    std::string m_source;
};
