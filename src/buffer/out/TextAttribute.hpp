/*++
Copyright (c) Microsoft Corporation

Module Name:
- TextAttribute.hpp

Abstract:
- contains data structure for run-length-encoding of text attribute data

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
--*/

#pragma once

class TextAttribute final
{
public:
    constexpr TextAttribute() noexcept :
        _wAttrLegacy{ 0 },
        _fUseRgbColor{ false },
        _rgbForeground{ RGB(0, 0, 0) },
        _rgbBackground{ RGB(0, 0, 0) }
    {
    }

    constexpr TextAttribute(const WORD wLegacyAttr) noexcept :
        _wAttrLegacy{ wLegacyAttr },
        _fUseRgbColor{ false },
        _rgbForeground{ RGB(0, 0, 0) },
        _rgbBackground{ RGB(0, 0, 0) }
    {
    }

    constexpr TextAttribute(const COLORREF rgbForeground, const COLORREF rgbBackground) noexcept :
        _wAttrLegacy{ 0 },
        _fUseRgbColor{ true },
        _rgbForeground{ rgbForeground },
        _rgbBackground{ rgbBackground }
    {
    }

    WORD GetLegacyAttributes() const noexcept;

    COLORREF CalculateRgbForeground() const;
    COLORREF CalculateRgbBackground() const;

    COLORREF GetRgbForeground() const;
    COLORREF GetRgbBackground() const;

    bool IsLeadingByte() const noexcept;
    bool IsTrailingByte() const noexcept;

    bool IsTopHorizontalDisplayed() const noexcept;
    bool IsBottomHorizontalDisplayed() const noexcept;
    bool IsLeftVerticalDisplayed() const noexcept;
    bool IsRightVerticalDisplayed() const noexcept;

    void SetFromLegacy(const WORD wLegacy) noexcept;
    void SetMetaAttributes(const WORD wMeta) noexcept;

    friend bool operator==(const TextAttribute& a, const TextAttribute& b) noexcept;
    friend bool operator!=(const TextAttribute& a, const TextAttribute& b) noexcept;
    friend bool operator==(const TextAttribute& attr, const WORD& legacyAttr) noexcept;
    friend bool operator!=(const TextAttribute& attr, const WORD& legacyAttr) noexcept;
    friend bool operator==(const WORD& legacyAttr, const TextAttribute& attr) noexcept;
    friend bool operator!=(const WORD& legacyAttr, const TextAttribute& attr) noexcept;

    bool IsLegacy() const noexcept;

    void SetForeground(const COLORREF rgbForeground);
    void SetBackground(const COLORREF rgbBackground);
    void SetColor(const COLORREF rgbColor, const bool fIsForeground);

private:
    COLORREF _GetRgbForeground() const;
    COLORREF _GetRgbBackground() const;

    bool _IsReverseVideo() const noexcept;

    WORD _wAttrLegacy;
    bool _fUseRgbColor;
    COLORREF _rgbForeground;
    COLORREF _rgbBackground;

#ifdef UNIT_TESTING
    friend class TextBufferTests;
#endif
};

bool operator==(const TextAttribute& a, const TextAttribute& b) noexcept;
bool operator!=(const TextAttribute& a, const TextAttribute& b) noexcept;
bool operator==(const TextAttribute& attr, const WORD& legacyAttr) noexcept;
bool operator!=(const TextAttribute& attr, const WORD& legacyAttr) noexcept;
bool operator==(const WORD& legacyAttr, const TextAttribute& attr) noexcept;
bool operator!=(const WORD& legacyAttr, const TextAttribute& attr) noexcept;