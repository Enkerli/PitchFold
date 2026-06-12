/* ==================================== JUCER_BINARY_RESOURCE ====================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#include <cstring>

namespace BinaryData
{

//================== index.html ==================
static const unsigned char temp_binary_data_0[] =
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"  <meta charset=\"UTF-8\" />\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" />\n"
"  <title>PitchFold</title>\n"
"  <style>\n"
"    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }\n"
"    html, body, #root {\n"
"      width: 100%; height: 100%;\n"
"      background: #F5F0E8;\n"
"      font-family: 'Inter Tight', system-ui, sans-serif;\n"
"      -webkit-font-smoothing: antialiased;\n"
"      overflow: hidden;\n"
"    }\n"
"    /* Prevent iOS rubber-band scroll inside the plugin UI */\n"
"    body { overscroll-behavior: none; touch-action: none; }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <div id=\"root\"></div>\n"
"  <script src=\"bundle.js\"></script>\n"
"</body>\n"
"</html>\n";

const char* index_html = (const char*) temp_binary_data_0;

}

#include "BinaryDataWebUI.h"

namespace BinaryData
{

const char* getNamedResource (const char* resourceNameUTF8, int& numBytes);
const char* getNamedResource (const char* resourceNameUTF8, int& numBytes)
{
    unsigned int hash = 0;

    if (resourceNameUTF8 != nullptr)
        while (*resourceNameUTF8 != 0)
            hash = 31 * hash + (unsigned int) *resourceNameUTF8++;

    switch (hash)
    {
        case 0x2c834af8:  numBytes = 714; return index_html;
        case 0xa7cf5a26:  numBytes = 183165; return bundle_js;
        default: break;
    }

    numBytes = 0;
    return nullptr;
}

const char* namedResourceList[] =
{
    "index_html",
    "bundle_js"
};

const char* originalFilenames[] =
{
    "index.html",
    "bundle.js"
};

const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8)
{
    for (unsigned int i = 0; i < (sizeof (namedResourceList) / sizeof (namedResourceList[0])); ++i)
        if (strcmp (namedResourceList[i], resourceNameUTF8) == 0)
            return originalFilenames[i];

    return nullptr;
}

}
