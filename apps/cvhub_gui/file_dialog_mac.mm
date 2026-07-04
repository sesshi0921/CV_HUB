#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "file_dialog.hpp"
#include "resources/app_icon_rgba.hpp"

#include <algorithm>
#include <filesystem>

namespace cvhub::gui {
namespace {

NSString* toNSString(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

std::optional<std::string> fromUrl(NSURL* url) {
    if (url == nil)
        return std::nullopt;
    NSString* path = [url path];
    if (path == nil)
        return std::nullopt;
    return std::string([path UTF8String]);
}

void bringAppToFront() {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}

} // namespace

void applyNativeAppIcon() {
    @autoreleasepool {
        NSString* bundledIcon = [[NSBundle mainBundle] pathForResource:@"app_icon" ofType:@"icns"];
        if (bundledIcon != nil) {
            NSImage* icon = [[NSImage alloc] initWithContentsOfFile:bundledIcon];
            if (icon != nil) {
                [NSApp setApplicationIconImage:icon];
                return;
            }
        }

        constexpr int width = resources::kAppIconWidth;
        constexpr int height = resources::kAppIconHeight;
        const auto& rgba = resources::kAppIconRgba;

        NSBitmapImageRep* rep =
            [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                    pixelsWide:width
                                                    pixelsHigh:height
                                                 bitsPerSample:8
                                               samplesPerPixel:4
                                                      hasAlpha:YES
                                                      isPlanar:NO
                                                colorSpaceName:NSDeviceRGBColorSpace
                                                   bytesPerRow:width * 4
                                                  bitsPerPixel:32];
        if (rep == nil)
            return;

        unsigned char* data = [rep bitmapData];
        std::copy(rgba.begin(), rgba.end(), data);

        NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
        [image addRepresentation:rep];
        [NSApp setApplicationIconImage:image];
    }
}

std::optional<std::string> openJsonFileDialog(const std::string& currentPath) {
    @autoreleasepool {
        bringAppToFront();
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setTitle:@"Load Graph JSON"];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        if (@available(macOS 11.0, *)) {
            [panel setAllowedContentTypes:@[ [UTType typeWithFilenameExtension:@"json"] ]];
        } else {
            [panel setAllowedFileTypes:@[ @"json" ]];
        }
        if (!currentPath.empty()) {
            std::filesystem::path p{currentPath};
            if (p.has_parent_path())
                [panel
                    setDirectoryURL:[NSURL fileURLWithPath:toNSString(p.parent_path().string())]];
        }
        if ([panel runModal] != NSModalResponseOK)
            return std::nullopt;
        return fromUrl([panel URL]);
    }
}

std::optional<std::string> saveJsonFileDialog(const std::string& currentPath) {
    @autoreleasepool {
        bringAppToFront();
        NSSavePanel* panel = [NSSavePanel savePanel];
        [panel setTitle:@"Save Graph JSON"];
        [panel setCanCreateDirectories:YES];
        if (@available(macOS 11.0, *)) {
            [panel setAllowedContentTypes:@[ [UTType typeWithFilenameExtension:@"json"] ]];
        } else {
            [panel setAllowedFileTypes:@[ @"json" ]];
        }
        std::filesystem::path p = currentPath.empty() ? std::filesystem::path{"graph.json"}
                                                      : std::filesystem::path{currentPath};
        if (p.has_parent_path())
            [panel setDirectoryURL:[NSURL fileURLWithPath:toNSString(p.parent_path().string())]];
        [panel setNameFieldStringValue:toNSString(p.filename().empty() ? std::string{"graph.json"}
                                                                       : p.filename().string())];
        if ([panel runModal] != NSModalResponseOK)
            return std::nullopt;
        auto selected = fromUrl([panel URL]);
        if (!selected)
            return std::nullopt;
        std::filesystem::path out{*selected};
        if (out.extension() != ".json")
            out += ".json";
        return out.string();
    }
}

} // namespace cvhub::gui
