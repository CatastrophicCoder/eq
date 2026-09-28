#include "ui/ResponseDisplay.h"
#include "ui/BandPanel.h"
#include "ui/SpectrumColour.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE ("Band wavelengths run evenly from violet to red", "[colour]")
{
    CHECK_THAT (SpectrumColour::wavelengthForBand (1, 16), WithinAbs (390.0, 1e-9));
    CHECK_THAT (SpectrumColour::wavelengthForBand (16, 16), WithinAbs (645.0, 1e-9));

    // 645 nm, not 700: the model is pure red from 645 nm up, so 700 made bands 14-16 identical
    // (decision 2026-09-28).
    const auto step = SpectrumColour::wavelengthForBand (2, 16) - SpectrumColour::wavelengthForBand (1, 16);
    CHECK_THAT (step, WithinAbs (17.0, 1e-9));
    for (int b = 2; b <= 16; ++b)
        CHECK_THAT (SpectrumColour::wavelengthForBand (b, 16) - SpectrumColour::wavelengthForBand (b - 1, 16),
                    WithinAbs (step, 1e-9));
}

TEST_CASE ("Wavelength colours follow the visible spectrum", "[colour]")
{
    auto c = [] (double nm) { return SpectrumColour::fromWavelength (nm); };

    // Primaries where the spectrum is unambiguous.
    CHECK (c (450.0).getBlue() > 200);   // blue
    CHECK (c (450.0).getRed() < 60);
    CHECK (c (530.0).getGreen() > 200);  // green
    CHECK (c (530.0).getBlue() < 60);
    CHECK (c (580.0).getRed() > 200);    // yellow: red and green
    CHECK (c (580.0).getGreen() > 200);
    CHECK (c (650.0).getRed() > 200);    // red
    CHECK (c (650.0).getGreen() < 30);

    // Violet: red and blue, no green, dimmed at the edge of vision.
    CHECK (c (390.0).getGreen() == 0);
    CHECK (c (390.0).getBlue() > c (390.0).getRed());
    CHECK (c (390.0).getBrightness() < c (450.0).getBrightness());

    // Outside the visible range: black.
    CHECK (c (350.0) == juce::Colour (0xff000000));
    CHECK (c (800.0) == juce::Colour (0xff000000));
}

TEST_CASE ("Band colours go from dark violet (band 1) to red (band 16)", "[colour]")
{
    for (int b = 1; b <= 16; ++b)
        CHECK (ResponseDisplay::bandColour (b) == SpectrumColour::fromWavelength (SpectrumColour::wavelengthForBand (b, 16)));

    const auto first = ResponseDisplay::bandColour (1);
    const auto last = ResponseDisplay::bandColour (16);

    CHECK (first.getGreen() == 0);
    CHECK (first.getBlue() > first.getRed());
    CHECK (first.getBrightness() < ResponseDisplay::bandColour (8).getBrightness());   // dark violet
    CHECK (last.getRed() > 200);
    CHECK (last.getGreen() < 30);
    CHECK (last.getBlue() == 0);

    // Every band has its own colour.
    for (int a = 1; a <= 16; ++a)
        for (int b = a + 1; b <= 16; ++b)
        {
            const auto x = ResponseDisplay::bandColour (a), y = ResponseDisplay::bandColour (b);
            const auto distance = std::hypot (x.getRed() - y.getRed(), x.getGreen() - y.getGreen(), x.getBlue() - y.getBlue());
            INFO ("bands " << a << " and " << b << " differ by " << distance);
            CHECK (distance > 20.0);
        }

    // Hue moves from violet towards red without turning back (violet ~0.75, red 0).
    for (int b = 2; b <= 16; ++b)
    {
        INFO ("band " << b);
        CHECK (ResponseDisplay::bandColour (b).getHue() <= ResponseDisplay::bandColour (b - 1).getHue() + 1e-3f);
    }
}

namespace
{
    /** WCAG 2 relative luminance and contrast ratio. */
    double luminance (juce::Colour c)
    {
        auto lin = [] (juce::uint8 v)
        {
            const auto s = v / 255.0;
            return s <= 0.03928 ? s / 12.92 : std::pow ((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * lin (c.getRed()) + 0.7152 * lin (c.getGreen()) + 0.0722 * lin (c.getBlue());
    }

    double contrast (juce::Colour a, juce::Colour b)
    {
        const auto la = luminance (a), lb = luminance (b);
        return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
    }
}

TEST_CASE ("Tab text uses a readable tint of the band colour", "[colour]")
{
    const auto background = BandPanel::tabBackground();

    for (int b = 1; b <= 16; ++b)
    {
        const auto band = ResponseDisplay::bandColour (b);
        const auto text = BandPanel::tabTextColour (b);
        INFO ("band " << b << " contrast " << contrast (text, background));

        CHECK_THAT (text.getHue(), WithinAbs (band.getHue(), 0.02));   // same hue family
        CHECK (contrast (text, background) >= 4.5);                    // WCAG AA for small text
    }
}
