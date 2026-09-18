#include "FileDialog.h"

#import <Cocoa/Cocoa.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {

NSMutableArray<NSString*>* MakeTypes(const std::vector<std::string>& extensions) {
    NSMutableArray<NSString*>* types = [NSMutableArray array];

    for (const std::string& extension : extensions) {
        [types addObject:[NSString stringWithUTF8String:extension.c_str()]];
    }

    return types;
}

NSString* MakeString(const std::string& text) {
    return [NSString stringWithUTF8String:text.c_str()];
}

} // namespace

namespace FileDialog {

std::string OpenFile(const std::string& title,
                     const std::vector<std::string>& extensions) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];

        [panel setTitle:MakeString(title)];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setAllowedFileTypes:MakeTypes(extensions)];

        if ([panel runModal] != NSModalResponseOK) {
            return {};
        }

        NSURL* url = [[panel URLs] objectAtIndex:0];

        return std::string([[url path] UTF8String]);
    }
}

std::string SaveFile(const std::string& title,
                     const std::string& defaultName,
                     const std::vector<std::string>& extensions) {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];

        [panel setTitle:MakeString(title)];
        [panel setNameFieldStringValue:MakeString(defaultName)];
        [panel setAllowedFileTypes:MakeTypes(extensions)];

        if ([panel runModal] != NSModalResponseOK) {
            return {};
        }

        NSURL* url = [panel URL];

        return std::string([[url path] UTF8String]);
    }
}

} // namespace FileDialog

#pragma clang diagnostic pop
