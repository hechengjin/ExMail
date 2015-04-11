/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "gfxAndroidPlatform.h"
#include "mozilla/gfx/2D.h"

#include "gfxFT2FontList.h"
#include "gfxImageSurface.h"
#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"

#include "cairo.h"

#include "ft2build.h"
#include FT_FREETYPE_H
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

static FT_Library gPlatformFTLibrary = NULL;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoFonts" , ## args)

gfxAndroidPlatform::gfxAndroidPlatform()
{
    FT_Init_FreeType(&gPlatformFTLibrary);

    nsCOMPtr<nsIScreenManager> screenMgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
    nsCOMPtr<nsIScreen> screen;
    screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
    mScreenDepth = 24;
    screen->GetColorDepth(&mScreenDepth);

    mOffscreenFormat = mScreenDepth == 16
                       ? gfxASurface::ImageFormatRGB16_565
                       : gfxASurface::ImageFormatRGB24;
}

gfxAndroidPlatform::~gfxAndroidPlatform()
{
    cairo_debug_reset_static_data();

    FT_Done_FreeType(gPlatformFTLibrary);
    gPlatformFTLibrary = NULL;
}

already_AddRefed<gfxASurface>
gfxAndroidPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                      gfxASurface::gfxContentType contentType)
{
    nsRefPtr<gfxASurface> newSurface;
    newSurface = new gfxImageSurface(size, OptimalFormatForContent(contentType));

    return newSurface.forget();
}

nsresult
gfxAndroidPlatform::GetFontList(nsIAtom *aLangGroup,
                                const nsACString& aGenericFamily,
                                nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup,
                                                         aGenericFamily,
                                                         aListOfFonts);
    return NS_OK;
}

void
gfxAndroidPlatform::GetFontList(InfallibleTArray<FontListEntry>* retValue)
{
    gfxFT2FontList::PlatformFontList()->GetFontList(retValue);
}

nsresult
gfxAndroidPlatform::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();
    return NS_OK;
}

nsresult
gfxAndroidPlatform::ResolveFontName(const nsAString& aFontName,
                                    FontResolverCallback aCallback,
                                    void *aClosure,
                                    bool& aAborted)
{
    nsAutoString resolvedName;
    if (!gfxPlatformFontList::PlatformFontList()->
             ResolveFontName(aFontName, resolvedName)) {
        aAborted = false;
        return NS_OK;
    }
    aAborted = !(*aCallback)(resolvedName, aClosure);
    return NS_OK;
}

nsresult
gfxAndroidPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName, aFamilyName);
    return NS_OK;
}

gfxPlatformFontList*
gfxAndroidPlatform::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxFT2FontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nullptr;
}

bool
gfxAndroidPlatform::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_OPENTYPE |
                        gfxUserFontSet::FLAG_FORMAT_WOFF |
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE)) {
        return true;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return false;
    }

    // no format hint set, need to look at data
    return true;
}

gfxFontGroup *
gfxAndroidPlatform::CreateFontGroup(const nsAString &aFamilies,
                               const gfxFontStyle *aStyle,
                               gfxUserFontSet* aUserFontSet)
{
    return new gfxFontGroup(aFamilies, aStyle, aUserFontSet);
}

FT_Library
gfxAndroidPlatform::GetFTLibrary()
{
    return gPlatformFTLibrary;
}

gfxFontEntry* 
gfxAndroidPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                     const uint8_t *aFontData, uint32_t aLength)
{
    return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(aProxyEntry,
                                                                     aFontData,
                                                                     aLength);
}

RefPtr<ScaledFont>
gfxAndroidPlatform::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
    NativeFont nativeFont;
    if (aTarget->GetType() == BACKEND_CAIRO) {
        nativeFont.mType = NATIVE_FONT_CAIRO_FONT_FACE;
        nativeFont.mFont = NULL;
        return Factory::CreateScaledFontWithCairo(nativeFont, aFont->GetAdjustedSize(), aFont->GetCairoScaledFont());
    }
 
    NS_ASSERTION(aFont->GetType() == gfxFont::FONT_TYPE_FT2, "Expecting Freetype font");
    nativeFont.mType = NATIVE_FONT_SKIA_FONT_FACE;
    nativeFont.mFont = static_cast<gfxFT2FontBase*>(aFont)->GetFontOptions();
    return Factory::CreateScaledFontForNativeFont(nativeFont, aFont->GetAdjustedSize());
}

bool
gfxAndroidPlatform::FontHintingEnabled()
{
    // In "mobile" builds, we sometimes use non-reflow-zoom, so we
    // might not want hinting.  Let's see.
#ifdef MOZ_USING_ANDROID_JAVA_WIDGETS
    // On android-java, we currently only use gecko to render web
    // content that can always be be non-reflow-zoomed.  So turn off
    // hinting.
    // 
    // XXX when gecko-android-java is used as an "app runtime", we'll
    // want to re-enable hinting.
    return false;
#else
    // Otherwise, enable hinting unless we're in a content process
    // that might be used for non-reflowing zoom.
    return (XRE_GetProcessType() != GeckoProcessType_Content ||
            (ContentChild::GetSingleton()->IsForApp() &&
             !ContentChild::GetSingleton()->IsForBrowser()));
#endif //  MOZ_USING_ANDROID_JAVA_WIDGETS
}

int
gfxAndroidPlatform::GetScreenDepth() const
{
    return mScreenDepth;
}
