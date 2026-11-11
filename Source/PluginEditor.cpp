#include "PluginEditor.h"

namespace
{
// --- palette: felt-tip greens / pinks / custard on 70s enamel and chrome ---------------
const juce::Colour ink       { 0xff1d1a14 };
const juce::Colour limeHi    { 0xffa9c84a };
const juce::Colour limeLo    { 0xff7c9c29 };
const juce::Colour pink      { 0xfff07fae };
const juce::Colour pinkDark  { 0xffb8467a };
const juce::Colour custard   { 0xfff4d870 };
const juce::Colour cream     { 0xfff6edc9 };
const juce::Colour olive     { 0xff4f7a22 };

juce::Font makeFont (float height, int style = juce::Font::bold)
{
    return juce::Font (juce::FontOptions (height, style));
}

// Perimeter of a rounded rectangle, sampled every ~step px.
std::vector<juce::Point<float>> perimeter (juce::Rectangle<float> r, float cr, float step)
{
    std::vector<juce::Point<float>> pts;
    auto edge = [&] (juce::Point<float> a, juce::Point<float> b)
    {
        const int n = juce::jmax (1, (int) (a.getDistanceFrom (b) / step));
        for (int i = 0; i < n; ++i)
            pts.push_back (a + (b - a) * ((float) i / (float) n));
    };
    auto arc = [&] (juce::Point<float> c, float a0, float a1)
    {
        const int n = juce::jmax (2, (int) (cr * juce::MathConstants<float>::halfPi / step));
        for (int i = 0; i < n; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) n;
            pts.push_back ({ c.x + cr * std::cos (a), c.y + cr * std::sin (a) });
        }
    };
    const float hp = juce::MathConstants<float>::halfPi, pi = juce::MathConstants<float>::pi;
    const auto l = r.getX(), t = r.getY(), rt = r.getRight(), b = r.getBottom();
    edge ({ l + cr, t }, { rt - cr, t });   arc ({ rt - cr, t + cr }, -hp, 0.f);
    edge ({ rt, t + cr }, { rt, b - cr });  arc ({ rt - cr, b - cr }, 0.f, hp);
    edge ({ rt - cr, b }, { l + cr, b });   arc ({ l + cr, b - cr }, hp, pi);
    edge ({ l, b - cr }, { l, t + cr });    arc ({ l + cr, t + cr }, pi, pi + hp);
    return pts;
}

// A hand-inked closed outline: jittered points joined with smooth curves.
juce::Path wobblyOutline (juce::Rectangle<float> r, float cr, int frame, int seed, float amp)
{
    auto pts = perimeter (r, cr, 15.f);
    juce::Random rnd (seed * 7919 + frame * 104729);
    for (auto& p : pts)
        p += { (rnd.nextFloat() - 0.5f) * 2.f * amp, (rnd.nextFloat() - 0.5f) * 2.f * amp };

    juce::Path path;
    const auto mid = [] (juce::Point<float> a, juce::Point<float> b) { return (a + b) * 0.5f; };
    path.startNewSubPath (mid (pts.back(), pts.front()));
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& cur = pts[i];
        const auto& nxt = pts[(i + 1) % pts.size()];
        const auto m = mid (cur, nxt);
        path.quadraticTo (cur.x, cur.y, m.x, m.y);
    }
    path.closeSubPath();
    return path;
}

// A hand-inked open wobbly line.
juce::Path wobblyLine (juce::Point<float> a, juce::Point<float> b, int frame, int seed, float amp)
{
    juce::Random rnd (seed * 6151 + frame * 15485863);
    const int n = juce::jmax (3, (int) (a.getDistanceFrom (b) / 14.f));
    juce::Path p;
    juce::Point<float> prev = a;
    p.startNewSubPath (a);
    for (int i = 1; i <= n; ++i)
    {
        auto pt = a + (b - a) * ((float) i / (float) n);
        if (i < n)
            pt += { (rnd.nextFloat() - 0.5f) * 2.f * amp * 0.4f, (rnd.nextFloat() - 0.5f) * 2.f * amp };
        p.quadraticTo (prev.x, prev.y, (prev.x + pt.x) * 0.5f, (prev.y + pt.y) * 0.5f);
        prev = pt;
    }
    p.lineTo (b);
    return p;
}

void strokeInk (juce::Graphics& g, const juce::Path& p, float width, juce::Colour c = ink)
{
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void chromeDisc (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff7f7f2), c.x - r * 0.5f, c.y - r * 0.6f,
                                             juce::Colour (0xff5d5d5a), c.x + r * 0.6f, c.y + r * 0.8f, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);
}

void hexNut (juce::Graphics& g, juce::Point<float> c, float r)
{
    juce::Path hex;
    hex.addPolygon (c, 6, r, 0.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeeeea), c.x - r, c.y - r, juce::Colour (0xff6b6b68), c.x + r, c.y + r, false));
    g.fillPath (hex);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (hex, juce::PathStrokeType (1.2f));
    g.setColour (juce::Colour (0xff1a1a18));
    g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.45f, r * 0.9f, r * 0.9f);
}
} // namespace

// ------------------------------------------------------------------------------------------
void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = bounds.getCentre();
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 22.f;
    const auto cap = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto point = [&] (float angle, float rad) { return juce::Point<float> (c.x + rad * std::sin (angle), c.y - rad * std::cos (angle)); };

    // printed scale, 0..10
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.f;
        g.setColour (ink);
        g.drawLine (juce::Line<float> (point (a, r + 6.f), point (a, r + (i % 5 == 0 ? 15.f : 11.f))), i % 5 == 0 ? 2.2f : 1.4f);
        if (i % 5 == 0)
        {
            g.setFont (makeFont (11.f));
            const auto tp = point (a, r + 25.f);
            g.drawText (juce::String (i), juce::Rectangle<float> (18.f, 14.f).withCentre (tp), juce::Justification::centred, false);
        }
    }

    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - r + 3.f, c.y - r + 6.f, r * 2.f, r * 2.f);

    // chrome skirt
    chromeDisc (g, c, r);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, 1.4f);

    // fluted cap
    const float cr = r * 0.82f;
    g.setGradientFill (juce::ColourGradient (cap.brighter (0.35f), c.x - cr * 0.6f, c.y - cr * 0.7f,
                                             cap.darker (0.45f), c.x + cr * 0.7f, c.y + cr * 0.9f, false));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f);
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 28.f;
        g.drawLine (juce::Line<float> (point (a, cr * 0.9f), point (a, cr)), 1.2f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f, 1.2f);

    // dished centre
    const float dr = cr * 0.62f;
    g.setGradientFill (juce::ColourGradient (cap.darker (0.25f), c.x - dr * 0.4f, c.y - dr * 0.5f,
                                             cap.brighter (0.2f), c.x + dr * 0.5f, c.y + dr * 0.7f, false));
    g.fillEllipse (c.x - dr, c.y - dr, dr * 2.f, dr * 2.f);

    // pointer
    const float a = startAngle + (endAngle - startAngle) * pos;
    g.setColour (cream);
    g.drawLine (juce::Line<float> (point (a, dr * 0.25f), point (a, cr * 0.97f)), 4.5f);
    g.setColour (ink.withAlpha (0.6f));
    g.drawLine (juce::Line<float> (point (a, dr * 0.25f), point (a, cr * 0.97f)), 1.0f);
}

void FootSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (2.f);
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (c.x - r + 2.f, c.y - r + 5.f, r * 2.f, r * 2.f);

    hexNut (g, c, r);

    const float dome = r * (down ? 0.60f : 0.66f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbfbf7), c.x - dome * 0.5f, c.y - dome * 0.6f,
                                             juce::Colour (0xff6d6d6a), c.x + dome * 0.6f, c.y + dome * 0.8f, false));
    g.fillEllipse (c.x - dome, c.y - dome, dome * 2.f, dome * 2.f);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.45f : 0.25f));
    g.fillEllipse (c.x - dome * 0.55f, c.y - dome * 0.8f, dome * 0.8f, dome * 0.45f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (c.x - dome, c.y - dome, dome * 2.f, dome * 2.f, 1.3f);
}

// ------------------------------------------------------------------------------------------
RoobarbEditor::RoobarbEditor (RoobarbAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      roobarbAttach (p.apvts, "roobarb", roobarbKnob),
      custardAttach (p.apvts, "custard", custardKnob),
      bypassAttach (p.apvts, "bypass", footSwitch)
{
    setLookAndFeel (&lnf);

    for (auto* k : { &roobarbKnob, &custardKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k->setMouseDragSensitivity (180);
        addAndMakeVisible (*k);
    }
    roobarbKnob.setColour (juce::Slider::rotarySliderFillColourId, olive);
    custardKnob.setColour (juce::Slider::rotarySliderFillColourId, pink);
    roobarbKnob.setDoubleClickReturnValue (true, 0.55);
    custardKnob.setDoubleClickReturnValue (true, 0.5);
    roobarbKnob.setTooltip ("Roobarb - the fuzz");
    custardKnob.setTooltip ("Custard - the jaunty top voice");

    addAndMakeVisible (footSwitch);

    setSize (360, 580);
    startTimerHz (8);
}

RoobarbEditor::~RoobarbEditor() { setLookAndFeel (nullptr); }

void RoobarbEditor::timerCallback()
{
    boilFrame = (boilFrame + 1) % 3;
    repaint();
}

void RoobarbEditor::resized()
{
    body = juce::Rectangle<float> (24.f, 22.f, 312.f, 536.f);
    plate = juce::Rectangle<float> (46.f, 52.f, 268.f, 92.f);

    roobarbKnob.setBounds (juce::Rectangle<int> (140, 140).withCentre ({ 105, 255 }));
    custardKnob.setBounds (juce::Rectangle<int> (140, 140).withCentre ({ 255, 255 }));
    footSwitch.setBounds (juce::Rectangle<int> (110, 110).withCentre ({ 180, 470 }));

    buildTextures();
}

// Hammered enamel, painted once per resize.
void RoobarbEditor::buildTextures()
{
    bodyTexture = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (bodyTexture);

    juce::Path shape;
    shape.addRoundedRectangle (body, 22.f);
    g.saveState();
    g.reduceClipRegion (shape);

    g.setGradientFill (juce::ColourGradient::vertical (limeHi, body.getY(), limeLo, body.getBottom()));
    g.fillRect (body);

    juce::Random rnd (1974);
    for (int i = 0; i < 2600; ++i)
    {
        const float px = body.getX() + rnd.nextFloat() * body.getWidth();
        const float py = body.getY() + rnd.nextFloat() * body.getHeight();
        const float pr = 1.2f + rnd.nextFloat() * 2.6f;
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillEllipse (px, py, pr, pr);
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.fillEllipse (px - 0.9f, py - 0.9f, pr * 0.7f, pr * 0.7f);
    }

    // soft vignette so the enclosure looks domed
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, body.getCentreX(), body.getCentreY(),
                                             juce::Colours::black.withAlpha (0.28f), body.getX(), body.getY(), true));
    g.fillRect (body);
    g.restoreState();
}

void RoobarbEditor::paint (juce::Graphics& g)
{
    // stage floor
    g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xff2b211a), 0.f, juce::Colour (0xff17110d), (float) getHeight()));
    g.fillAll();

    // pedal shadow + enamel
    juce::Path bodyPath;
    bodyPath.addRoundedRectangle (body, 22.f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.65f), 22, { 0, 10 }).drawForPath (g, bodyPath);
    g.drawImageAt (bodyTexture, 0, 0);

    // 70s stripes: pink + custard, hand-ruled
    g.saveState();
    g.reduceClipRegion (bodyPath);
    const float sy = 338.f;
    strokeInk (g, wobblyLine ({ body.getX() - 4.f, sy }, { body.getRight() + 4.f, sy + 3.f }, boilFrame, 11, 2.2f), 15.f, pink);
    strokeInk (g, wobblyLine ({ body.getX() - 4.f, sy + 17.f }, { body.getRight() + 4.f, sy + 20.f }, boilFrame, 12, 2.2f), 15.f, custard);
    strokeInk (g, wobblyLine ({ body.getX() - 4.f, sy - 8.f }, { body.getRight() + 4.f, sy - 5.f }, boilFrame, 13, 1.6f), 2.5f);
    strokeInk (g, wobblyLine ({ body.getX() - 4.f, sy + 28.f }, { body.getRight() + 4.f, sy + 31.f }, boilFrame, 14, 1.6f), 2.5f);
    g.restoreState();

    // title plate
    {
        auto pr = plate;
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (pr.translated (2.5f, 4.f), 12.f);
        g.setColour (custard);
        g.fillRoundedRectangle (pr, 12.f);
        strokeInk (g, wobblyOutline (pr, 12.f, boilFrame, 3, 1.5f), 3.2f);

        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-0.018f, pr.getCentreX(), pr.getCentreY()));
        g.setColour (ink);
        auto title = makeFont (36.f);
        title.setExtraKerningFactor (0.04f);
        g.setFont (title);
        g.drawText ("SCUZZ-O-TONE", pr.withTrimmedBottom (28.f).toNearestInt(), juce::Justification::centred, false);
        auto sub = makeFont (11.5f);
        sub.setExtraKerningFactor (0.22f);
        g.setFont (sub);
        g.setColour (pinkDark.darker (0.4f));
        g.drawText ("FUZZ & JAUNT UNIT  -  MK I", pr.withTrimmedTop (58.f).toNearestInt(), juce::Justification::centred, false);
        g.restoreState();
    }

    // knob captions
    auto caption = [&] (const juce::String& big, const juce::String& small, float cx, float rot)
    {
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (rot, cx, 297.f));
        g.setColour (ink);
        auto f = makeFont (21.f);
        f.setExtraKerningFactor (0.05f);
        g.setFont (f);
        g.drawText (big, juce::Rectangle<float> (130.f, 24.f).withCentre ({ cx, 295.f }).toNearestInt(), juce::Justification::centred, false);
        auto f2 = makeFont (10.f);
        f2.setExtraKerningFactor (0.25f);
        g.setFont (f2);
        g.setColour (ink.withAlpha (0.75f));
        g.drawText (small, juce::Rectangle<float> (130.f, 12.f).withCentre ({ cx, 311.f }).toNearestInt(), juce::Justification::centred, false);
        g.restoreState();
    };
    caption ("ROOBARB", "F U Z Z", 105.f, 0.02f);
    caption ("CUSTARD", "J A U N T", 255.f, -0.025f);

    // status LED (lit when the pedal is engaged)
    {
        const bool engaged = ! footSwitch.getToggleState();
        const juce::Point<float> c { 180.f, 398.f };
        if (engaged)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0x88ff3b2a), c.x, c.y, juce::Colours::transparentBlack, c.x + 26.f, c.y, true));
            g.fillEllipse (c.x - 26.f, c.y - 26.f, 52.f, 52.f);
        }
        chromeDisc (g, c, 12.f);
        g.setColour (engaged ? juce::Colour (0xffff4a36) : juce::Colour (0xff4a1510));
        g.fillEllipse (c.x - 8.f, c.y - 8.f, 16.f, 16.f);
        g.setColour (juce::Colours::white.withAlpha (engaged ? 0.7f : 0.25f));
        g.fillEllipse (c.x - 4.f, c.y - 5.5f, 5.f, 4.f);
    }

    // bottom stencil
    g.setColour (ink.withAlpha (0.85f));
    auto stencil = makeFont (10.f);
    stencil.setExtraKerningFactor (0.2f);
    g.setFont (stencil);
    g.drawText ("MADE IN ENGLAND  -  1974", juce::Rectangle<int> (24, 530, 312, 14), juce::Justification::centred, false);

    // jack sockets poking out of the sides
    for (float y : { 255.f, 400.f })
    {
        hexNut (g, { body.getX() - 1.f, y }, 11.f);
        hexNut (g, { body.getRight() + 1.f, y }, 11.f);
    }

    // corner screws
    const juce::Point<float> screws[] = { { 48.f, 44.f }, { 312.f, 44.f }, { 48.f, 536.f }, { 312.f, 536.f } };
    for (auto p : screws)
    {
        chromeDisc (g, p, 6.5f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (p.x - 4.f, p.y - 1.5f, p.x + 4.f, p.y + 1.5f, 1.4f);
    }

    // enclosure outline, inked and boiling
    strokeInk (g, wobblyOutline (body.reduced (1.f), 22.f, boilFrame, 1, 1.4f), 4.f);
    strokeInk (g, wobblyOutline (body.reduced (7.f), 17.f, boilFrame, 2, 1.1f), 1.6f, ink.withAlpha (0.55f));
}
