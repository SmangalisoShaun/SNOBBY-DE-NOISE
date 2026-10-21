#include "PluginEditor.h"
using namespace juce;

namespace
{
    const char* faderIds[6]    = { "rumble", "body", "presence", "hissLow", "hissMid", "hissHigh" };
    const char* faderNames[6]  = { "RUMBLE", "BODY", "PRESENCE", "HISS LOW", "HISS MID", "HISS HIGH" };
    const char* faderRanges[6] = { "20-120 Hz", "120-500 Hz", "500-5k Hz", "5k-8k Hz", "8k-12k Hz", "12k-20k Hz" };
    const char* knobIds[3]     = { "attack", "release", "output" };
    const char* knobNames[3]   = { "ATTACK", "RELEASE", "OUTPUT" };

    const Colour ink (0xff1b2622);
    const Colour mint (0xff5dff9a);

    Rectangle<float> R (float x, float y, float w, float h) { return { x, y, w, h }; }
    Font fnt (float h, bool bold = true) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

    void txt (Graphics& g, const String& s, Rectangle<float> r, float h, Colour c,
              Justification j = Justification::centred, bool bold = true)
    {
        g.setColour (c);
        g.setFont (fnt (h, bold));
        g.drawText (s, r, j, false);
    }

    void lcd (Graphics& g, Rectangle<float> r, const String& s)
    {
        g.setColour (Colour (0xff050a07));
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (Colour (0x55ffffff));
        g.drawRoundedRectangle (r, 3.f, 1.f);
        txt (g, s, r, 12.f, mint);
    }

    void lamp (Graphics& g, float cx, float cy, float rad, Colour c, bool on)
    {
        if (on)
        {
            g.setColour (c.withAlpha (0.25f));
            g.fillEllipse (cx - rad * 2, cy - rad * 2, rad * 4, rad * 4);
        }
        g.setColour (Colour (0xff121212));
        g.fillEllipse (cx - rad - 2, cy - rad - 2, rad * 2 + 4, rad * 2 + 4);
        g.setColour (on ? c : Colour (0xff2b2b2b));
        g.fillEllipse (cx - rad, cy - rad, rad * 2, rad * 2);
        g.setColour (Colour (0x66ffffff));
        g.fillEllipse (cx - rad * 0.5f, cy - rad * 0.7f, rad * 0.8f, rad * 0.6f);
    }

    void drawUnit (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0, 10).reduced (6, 0), 8.f);

        for (bool left : { true, false })
        {
            auto e = Rectangle<float> (left ? r.getX() : r.getRight() - 20.f, r.getY(), 20.f, r.getHeight());
            g.setGradientFill (ColourGradient (Colour (0xff0a0a0b), e.getX(), 0, Colour (0xff2e2f33), e.getCentreX(), 0, false));
            g.fillRect (e);
            g.setColour (Colours::black);
            g.fillRoundedRectangle (e.getCentreX() - 5.5f, e.getCentreY() - 17.f, 11.f, 34.f, 6.f);
            g.setColour (Colour (0x30ffffff));
            g.drawRoundedRectangle (e.getCentreX() - 5.5f, e.getCentreY() - 16.f, 11.f, 34.f, 6.f, 1.f);
        }

        auto p = r.withTrimmedLeft (20).withTrimmedRight (20);
        g.setGradientFill (ColourGradient (Colour (0xffdfe2e4), 0, p.getY(), Colour (0xffaeb3b8), 0, p.getBottom(), false));
        g.fillRect (p);
        g.setColour (Colour (0x14ffffff));
        for (float x = p.getX(); x < p.getRight(); x += 3.f) g.fillRect (x, p.getY(), 1.f, p.getHeight());
        g.setColour (Colour (0xffffffff));
        g.fillRect (p.getX(), p.getY(), p.getWidth(), 1.f);
        g.setColour (Colour (0x48000000));
        g.fillRect (p.getX(), p.getBottom() - 3.f, p.getWidth(), 3.f);

        const float pts[4][2] = { { p.getX() + 8, p.getY() + 8 }, { p.getRight() - 8, p.getY() + 8 },
                                  { p.getX() + 8, p.getBottom() - 8 }, { p.getRight() - 8, p.getBottom() - 8 } };
        for (int i = 0; i < 4; ++i)
        {
            const float sx = pts[i][0], sy = pts[i][1];
            g.setColour (Colour (0x66000000));
            g.fillEllipse (sx - 6, sy - 4, 12, 12);
            g.setGradientFill (ColourGradient (Colours::white, sx - 2, sy - 3, Colour (0xff3a3a3a), sx + 5, sy + 5, true));
            g.fillEllipse (sx - 6, sy - 6, 12, 12);
            g.setColour (Colour (0xff222222));
            Path slot;
            slot.addRectangle (-4.f, -1.f, 8.f, 2.f);
            g.fillPath (slot, AffineTransform::rotation ((float) (i * 1.3 + 0.6)).translated (sx, sy));
        }
    }
}

//==============================================================================
void SnobbyLnF::drawLinearSlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                                  float, float, Slider::SliderStyle, Slider& s)
{
    const bool hiss = (bool) s.getProperties()["hiss"];
    const float cx = (float) x + (float) width * 0.5f;
    const Colour tick = hiss ? Colour (0xccf0fbf4) : Colour (0xcc1b2622);

    for (int i = 0; i <= 10; ++i)
    {
        const float ty = (float) y + 8.f + ((float) height - 16.f) * (float) i / 10.f;
        g.setColour (tick);
        g.fillRect (cx - 21.f, ty - 0.5f, 9.f, 1.f);
        g.fillRect (cx + 12.f, ty - 0.5f, 9.f, 1.f);
    }

    g.setColour (Colour (0x59ffffff));
    g.fillRoundedRectangle (cx - 4.5f, (float) y + 1.f, 9.f, (float) height, 4.5f);
    g.setColour (Colours::black);
    g.fillRoundedRectangle (cx - 4.f, (float) y, 8.f, (float) height, 4.f);

    auto cap = Rectangle<float> (cx - 17.f, sliderPos - 8.f, 34.f, 16.f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (cap.translated (0, 5.f).expanded (1.f, 0), 3.f);
    g.setGradientFill (ColourGradient (Colour (0xff6a6b70), cap.getX(), cap.getY(), Colour (0xff17181a), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.f);
    g.setColour (Colour (0x66ffffff));
    g.drawLine (cap.getX() + 2.f, cap.getY() + 0.5f, cap.getRight() - 2.f, cap.getY() + 0.5f, 1.f);
    g.setColour (Colour (0xfff3f0e6));
    g.fillRect (cap.getX() + 3.f, cap.getCentreY() - 1.f, cap.getWidth() - 6.f, 2.f);
}

void SnobbyLnF::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float pos,
                                  float startAngle, float endAngle, Slider&)
{
    const float r  = (float) jmin (width, height) * 0.5f - 2.f;
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (cx - r + 1.f, cy - r + 7.f, r * 2.f, r * 2.f);
    g.setColour (Colour (0xff0b0b0c));
    g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);

    for (int i = 0; i < 60; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 60.f;
        g.setColour ((i % 2) ? Colour (0xff4a4b4f) : Colour (0xff0b0b0c));
        g.drawLine (cx + (r - 4.f) * std::sin (a), cy - (r - 4.f) * std::cos (a),
                    cx + r * std::sin (a), cy - r * std::cos (a), 1.6f);
    }

    const float cr = r * 0.78f;
    g.setGradientFill (ColourGradient (Colour (0xff74757a), cx - cr * 0.4f, cy - cr * 0.5f,
                                       Colour (0xff0c0c0d), cx + cr * 0.7f, cy + cr * 0.9f, true));
    g.fillEllipse (cx - cr, cy - cr, cr * 2.f, cr * 2.f);
    g.setColour (Colour (0x55ffffff));
    g.drawEllipse (cx - cr, cy - cr, cr * 2.f, cr * 2.f, 1.f);

    const float a = startAngle + pos * (endAngle - startAngle);
    g.setColour (Colours::white);
    g.drawLine (cx + r * 0.28f * std::sin (a), cy - r * 0.28f * std::cos (a),
                cx + r * 0.7f * std::sin (a), cy - r * 0.7f * std::cos (a), 3.f);
}

void SnobbyLnF::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool, bool down)
{
    const bool on = b.getToggleState() || down;
    auto area = b.getLocalBounds().toFloat().reduced (1.f);
    auto body = area.withTrimmedBottom (6.f).translated (0.f, on ? 4.f : 0.f);

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (body.translated (0.f, on ? 3.f : 8.f), 5.f);
    g.setColour (Colour (0xff0b0b0c));
    g.fillRoundedRectangle (body.translated (0.f, on ? 1.f : 5.f), 5.f);
    g.setGradientFill (ColourGradient (Colour (0xff5a5b5f), 0, body.getY(), Colour (0xff1b1b1d), 0, body.getBottom(), false));
    g.fillRoundedRectangle (body, 5.f);
    g.setColour (Colour (0x59ffffff));
    g.drawLine (body.getX() + 3.f, body.getY() + 0.5f, body.getRight() - 3.f, body.getY() + 0.5f, 1.f);
    txt (g, b.getName(), body, 10.f, Colours::white);
}

//==============================================================================
void SnobbyFace::Meter::update (float lin, float dt)
{
    const float d = Decibels::gainToDecibels (lin, -90.f);
    if (d > pk) pk = d; else pk = jmax (-90.f, pk - dt * 26.f);
    if (pk >= hold) { hold = pk; hv = 1.4f; }
    else { hv -= dt; if (hv < 0.f) hold = jmax (-90.f, hold - dt * 14.f); }
    avg += ((d - 8.f) - avg) * jmin (1.f, dt * 3.3f);
    avg = jmax (avg, -90.f);
}

SnobbyFace::SnobbyFace (SnobbyAudioProcessor& p) : proc (p)
{
    for (auto& v : sIn)  v = -90.f;
    for (auto& v : sOut) v = -90.f;

    for (int i = 0; i < 6; ++i)
    {
        auto& s = faders[i];
        s.setSliderStyle (Slider::LinearVertical);
        s.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        s.setLookAndFeel (&lnf);
        s.getProperties().set ("hiss", i >= 3);
        addAndMakeVisible (s);
        fa[i] = std::make_unique<SA> (proc.apvts, faderIds[i], s);
        s.setDoubleClickReturnValue (true, 0.0);
    }

    for (int i = 0; i < 3; ++i)
    {
        auto& s = knobs[i];
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setLookAndFeel (&lnf);
        addAndMakeVisible (s);
        ka[i] = std::make_unique<SA> (proc.apvts, knobIds[i], s);
    }

    bypass.setName ("BYPASS");
    bypass.setClickingTogglesState (true);
    bypass.setLookAndFeel (&lnf);
    addAndMakeVisible (bypass);
    ba = std::make_unique<BA> (proc.apvts, "bypass", bypass);

    learn.setName ("LEARN HISS");
    learn.setLookAndFeel (&lnf);
    learn.onClick = [this] { proc.startLearn(); };
    addAndMakeVisible (learn);

    vuBtn.setName ("VU: LEVEL");
    vuBtn.setLookAndFeel (&lnf);
    vuBtn.onClick = [this]
    {
        grMode = ! grMode;
        vuBtn.setName (grMode ? "VU: GR" : "VU: LEVEL");
        repaint();
    };
    addAndMakeVisible (vuBtn);

    startTimerHz (30);
}

SnobbyFace::~SnobbyFace()
{
    for (auto& s : faders) s.setLookAndFeel (nullptr);
    for (auto& s : knobs) s.setLookAndFeel (nullptr);
    bypass.setLookAndFeel (nullptr);
    learn.setLookAndFeel (nullptr);
    vuBtn.setLookAndFeel (nullptr);
}

void SnobbyFace::resized()
{
    for (int i = 0; i < 6; ++i)
        faders[i].setBounds ((int) (46.f + (float) i * 104.6667f) + 30, 360, 44, 120);
    for (int i = 0; i < 3; ++i)
        knobs[i].setBounds (100 + 100 * i - 32, 554, 64, 64);
    bypass.setBounds (592, 564, 84, 40);
    learn.setBounds (592, 646, 84, 40);
    vuBtn.setBounds (420, 702, 100, 36);
}

void SnobbyFace::mouseDown (const MouseEvent& e)
{
    if (clipRect().contains (e.getPosition())) clip = false;
}

void SnobbyFace::setParam (const char* id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void SnobbyFace::timerCallback()
{
    const float dt = 1.f / 30.f;
    const float inLin  = proc.inLevel.exchange (0.f);
    const float outLin = proc.outLevel.exchange (0.f);
    inM.update (inLin, dt);
    outM.update (outLin, dt);
    if (inLin >= 0.999f || outLin >= 0.999f) clip = true;

    const float vuNow = proc.vuDb.load() + 18.f;     // 0 VU = -18 dBFS
    if (inM.pk > -45.f) { seen = true; voiceVu += (vuNow - voiceVu) * dt * 0.8f; }

    const float target = grMode ? proc.grDb.load() : jlimit (-20.f, 3.5f, vuNow);
    needle += (target - needle) * 0.25f;

    for (int k = 0; k < 24; ++k)
    {
        sIn[k]  += (proc.specIn[k].load()  - sIn[k])  * 0.35f;
        sOut[k] += (proc.specOut[k].load() - sOut[k]) * 0.35f;
    }

    if (proc.learnState.load() == 2)
    {
        setParam ("hissLow", -14.f);
        setParam ("hissMid", -26.f);
        setParam ("hissHigh", -30.f);
        proc.learnState.store (0);
    }
    repaint();
}

//==============================================================================
void SnobbyFace::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (Colour (0xff24262b), 360, 0, Colour (0xff0d0e10), 360, (float) baseH, false));
    g.fillAll();

    drawUnit (g, R (10, 10, 700, 506));
    drawUnit (g, R (10, 530, 700, 212));

    txt (g, "BOOMHYPE TECH", R (46, 20, 240, 24), 15.f, ink, Justification::centredLeft);
    txt (g, "SNOBBY DENOISE SD-1", R (260, 20, 200, 24), 10.f, Colour (0xff34403b));
    txt (g, "SN 000117 - 2026", R (500, 20, 174, 24), 9.f, Colour (0xff55605b), Justification::centredRight, false);

    drawLevels (g);
    drawScope (g);
    drawModules (g);
    drawBottom (g);
}

static void ladder (Graphics& g, float avg, float pk, float hold, Rectangle<float> r)
{
    const int n = 60;
    const float sw = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float db = -60.f + (float) i;
        Colour c = db >= -6.f ? Colour (0xffff4a3d) : db >= -12.f ? Colour (0xfff5b73c) : Colour (0xff31d27a);
        float a = db < avg ? 1.f : db < pk ? 0.4f : 0.13f;
        if (std::floor (hold) == db) a = 1.f;
        g.setColour (c.withAlpha (a));
        g.fillRect (r.getX() + (float) i * sw, r.getY(), jmax (1.f, sw - 1.f), r.getHeight());
    }
}

void SnobbyFace::drawLevels (Graphics& g)
{
    auto s = R (46, 52, 628, 92);
    g.setGradientFill (ColourGradient (Colour (0xff0b0d0e), 0, s.getY(), Colour (0xff040506), 0, s.getBottom(), false));
    g.fillRoundedRectangle (s, 10.f);
    g.setColour (Colour (0x66ffffff));
    g.drawRoundedRectangle (s.expanded (0.5f), 10.f, 1.f);

    const float x0 = 82.f, w = 580.f;
    auto xd = [&] (float db) { return x0 + (db + 60.f) / 60.f * w; };

    // 0 VU = -18 dBFS, target is 0 VU plus or minus 3 dB
    g.setColour (Colour (0x1a50e68c));
    g.fillRect (xd (-21.f), s.getY() + 2.f, xd (-15.f) - xd (-21.f), 36.f);

    txt (g, "IN",  R (52, s.getY() + 6.f, 28, 12), 10.f, Colour (0xffcfd9d3), Justification::centredLeft);
    txt (g, "OUT", R (52, s.getY() + 22.f, 28, 12), 10.f, Colour (0xffcfd9d3), Justification::centredLeft);
    ladder (g, inM.avg,  inM.pk,  inM.hold,  R (x0, s.getY() + 6.f, w, 12));
    ladder (g, outM.avg, outM.pk, outM.hold, R (x0, s.getY() + 22.f, w, 12));

    g.setColour (mint);
    g.fillRect (xd (-21.f), s.getY() + 38.f, xd (-15.f) - xd (-21.f), 2.f);
    txt (g, "0 VU = -18", R (xd (-27.f), s.getY() + 41.f, xd (-9.f) - xd (-27.f), 10), 8.f, Colour (0xff7fe0a8));
    for (int d : { -60, -48, -36, -24, -12, -6, 0 })
    {
        g.setColour (Colour (0xff9aa8a1));
        g.fillRect (xd ((float) d), s.getY() + 34.f, 1.f, 3.f);
        txt (g, String (d), R (xd ((float) d) - 16.f, s.getY() + 51.f, 32, 10), 9.f, Colour (0xff9aa8a1), Justification::centred, false);
    }

    String st; Colour sc;
    const bool hot = clip || outM.hold > -1.5f || voiceVu > 3.f;
    int adv = roundToInt (-voiceVu);
    if (! seen)                  { st = "NO SIGNAL"; sc = Colour (0xff9aa8a1); }
    else if (hot)                { adv = jmin (adv, -1); st = "TOO HOT  " + String (adv) + " dB"; sc = Colour (0xffff7a6e); }
    else if (voiceVu < -3.f)     { st = "TOO QUIET  +" + String (adv) + " dB"; sc = Colour (0xffffc46b); }
    else                         { st = "GOOD  " + String (voiceVu, 1) + " VU"; sc = mint; }

    lamp (g, 62.f, s.getY() + 77.f, 5.f, sc, seen);
    txt (g, st, R (74, s.getY() + 68.f, 260, 18), 11.f, sc, Justification::centredLeft);

    auto cr = clipRect().toFloat();
    g.setColour (Colour (0xff1c1f21));
    g.fillRoundedRectangle (cr, 4.f);
    g.setColour (Colour (0xff3a3e40));
    g.drawRoundedRectangle (cr, 4.f, 1.f);
    lamp (g, cr.getX() + 12.f, cr.getCentreY(), 4.f, Colour (0xffff4a3d), clip);
    txt (g, "CLIP RESET", R (cr.getX() + 20.f, cr.getY(), cr.getWidth() - 22.f, cr.getHeight()), 9.f, Colour (0xffd8ece0));
}

void SnobbyFace::drawScope (Graphics& g)
{
    auto bez = R (46, 156, 628, 142);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bez.translated (0, 4.f), 13.f);
    g.setGradientFill (ColourGradient (Colour (0xff0e0e0f), 0, bez.getY(), Colours::black, 0, bez.getBottom(), false));
    g.fillRoundedRectangle (bez, 13.f);
    g.setColour (Colour (0xb3ffffff));
    g.drawRoundedRectangle (bez.expanded (0.5f), 13.f, 1.f);

    auto sc = bez.reduced (6.f);
    Graphics::ScopedSaveState ss (g);
    Path clipP;
    clipP.addRoundedRectangle (sc, 8.f);
    g.reduceClipRegion (clipP);

    g.setGradientFill (ColourGradient (Colour (0xff0e0f10), 0, sc.getY(), Colour (0xff050606), 0, sc.getBottom(), false));
    g.fillRect (sc);

    const float W = sc.getWidth(), H = sc.getHeight();
    auto yOf = [&] (float db) { return sc.getY() + H * jlimit (0.f, 1.f, -db / 80.f); };

    g.setColour (Colour (0x2160d282));
    g.fillRect (sc.getX() + W * 0.5f, sc.getY(), W * 0.5f, H);

    for (int i = 1; i < 6; ++i)
    {
        g.setColour (i == 3 ? Colour (0xbf50dc8c) : Colour (0x14ffffff));
        g.fillRect (sc.getX() + W * (float) i / 6.f, sc.getY(), 1.f, H);
    }
    for (int d : { 20, 40, 60 })
    {
        const float y = yOf ((float) -d);
        g.setColour (Colour (0x12ffffff));
        g.fillRect (sc.getX(), y, W, 1.f);
        txt (g, "-" + String (d), R (sc.getX() + 6.f, y - 13.f, 30, 12), 10.f, Colour (0x59ffffff), Justification::centredLeft, false);
    }
    txt (g, "HISS", R (sc.getX() + W * 0.5f + 8.f, sc.getY() + 3.f, 40, 12), 10.f, Colour (0xff9ff0bf), Justification::centredLeft);

    auto makePath = [&] (const float* v)
    {
        std::vector<Point<float>> pts;
        pts.push_back ({ sc.getX(), yOf (v[0]) });
        for (int k = 0; k < 24; ++k) pts.push_back ({ sc.getX() + ((float) k + 0.5f) / 24.f * W, yOf (v[k]) });
        pts.push_back ({ sc.getRight(), yOf (v[23]) });
        Path p;
        p.startNewSubPath (pts[0]);
        for (size_t i = 1; i + 1 < pts.size(); ++i)
            p.quadraticTo (pts[i], (pts[i] + pts[i + 1]) * 0.5f);
        p.lineTo (pts.back());
        return p;
    };

    Path pin = makePath (sIn), pout = makePath (sOut);

    Path fin (pin);
    fin.lineTo (sc.getRight(), sc.getBottom()); fin.lineTo (sc.getX(), sc.getBottom()); fin.closeSubPath();
    g.setColour (Colour (0x1abebeb4));
    g.fillPath (fin);
    g.setColour (Colour (0x80c8c6bc));
    g.strokePath (pin, PathStrokeType (1.2f));

    Path fout (pout);
    fout.lineTo (sc.getRight(), sc.getBottom()); fout.lineTo (sc.getX(), sc.getBottom()); fout.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0x4746e68c), 0, sc.getY(), Colour (0x0046e68c), 0, sc.getBottom(), false));
    g.fillPath (fout);
    g.setColour (Colour (0x4046ff96));
    g.strokePath (pout, PathStrokeType (4.5f));
    g.setColour (Colour (0xff4dffa0));
    g.strokePath (pout, PathStrokeType (1.8f));

    // dashed lines: the noise floor the plugin is tracking in each band
    g.setColour (Colour (0xb36ef0a5));
    for (int b = 0; b < 6; ++b)
    {
        float nf = 0.f;
        for (int s4 = 0; s4 < 4; ++s4) nf += proc.noiseDisp[b * 4 + s4].load();
        nf *= 0.25f;
        const float y = yOf (nf);
        const float x1 = sc.getX() + W * (float) b / 6.f + 6.f, x2 = sc.getX() + W * (float) (b + 1) / 6.f - 6.f;
        for (float x = x1; x < x2; x += 9.f) g.drawLine (x, y, jmin (x + 5.f, x2), y, 1.f);
    }

    if (proc.learnState.load() == 1)
        txt (g, "LEARNING NOISE - KEEP SILENT", R (sc.getX(), sc.getY() + 4.f, W, 14), 10.f, Colour (0xffffc46b));

    g.setGradientFill (ColourGradient (Colour (0x24ffffff), sc.getX(), sc.getY(), Colour (0x00ffffff), sc.getX() + W * 0.35f, sc.getY() + H * 0.6f, false));
    g.fillRect (sc);
}

void SnobbyFace::drawModules (Graphics& g)
{
    const float y = 310.f, h = 202.f;
    auto hiss = R (360, y, 314, h);

    g.setGradientFill (ColourGradient (Colour (0xff1f7a4d), 0, y, Colour (0xff0f4a2d), 0, y + h, false));
    g.fillRoundedRectangle (hiss, 8.f);
    g.setColour (Colour (0x66000000));
    g.drawRoundedRectangle (hiss, 8.f, 2.f);
    g.setColour (Colour (0x33ffffff));
    g.drawLine (hiss.getX() + 8.f, y + 1.5f, hiss.getRight() - 8.f, y + 1.5f, 1.f);
    g.setColour (Colour (0xfff0f2f3));
    g.fillRect (357.f, y, 3.f, h);

    txt (g, "VOICE",     R (46, y + 3.f, 311, 16), 9.f, ink);
    txt (g, "HISS ZONE", R (360, y + 3.f, 314, 16), 9.f, Colour (0xffb9f5cf));
    g.setColour (Colour (0x40000000));
    g.fillRect (56.f, y + 20.f, 291.f, 1.f);
    g.setColour (Colour (0x4dffffff));
    g.fillRect (370.f, y + 20.f, 294.f, 1.f);

    for (int i = 0; i < 6; ++i)
    {
        const float cx = 46.f + (float) i * 104.6667f;
        const Colour tc = i >= 3 ? Colour (0xfff0fbf4) : ink;
        txt (g, faderNames[i],  R (cx, y + 24.f, 104.67f, 14), 10.f, tc);
        txt (g, faderRanges[i], R (cx, y + 38.f, 104.67f, 12), 8.f, tc.withAlpha (0.8f), Justification::centred, false);
        lcd (g, R (cx + (104.67f - 54.f) * 0.5f, y + 174.f, 54, 22), String (faders[i].getValue(), 1));
    }
}

void SnobbyFace::drawBottom (Graphics& g)
{
    for (int i = 0; i < 3; ++i)
    {
        const float cx = 100.f + 100.f * (float) i;
        txt (g, knobNames[i], R (cx - 45.f, 630, 90, 14), 10.f, ink);
        const double v = knobs[i].getValue();
        String s = i == 0 ? String (v, 1) + " ms"
                 : i == 1 ? String (roundToInt (v)) + " ms"
                          : String (v >= 0 ? "+" : "") + String (v, 1) + " dB";
        lcd (g, R (cx - 42.f, 648, 84, 22), s);
    }

    drawVU (g, R (370, 550, 200, 127), ! grMode);
    txt (g, grMode ? "GAIN REDUCTION" : "OUTPUT LEVEL", R (370, 682, 180, 14), 10.f, ink);

    const bool byp = proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    lamp (g, 634.f, 552.f, 6.f, Colour (0xff43e273), ! byp);
    txt (g, "CHANNEL IN / OUT", R (570, 606, 128, 12), 8.f, Colour (0xff3f4a45));

    lamp (g, 634.f, 634.f, 6.f, Colour (0xffffb341), proc.learnState.load() == 1);
    txt (g, "PLAY SILENCE, THEN PRESS", R (550, 690, 168, 12), 8.f, Colour (0xff3f4a45));
}

void SnobbyFace::drawVU (Graphics& g, Rectangle<float> r, bool levelMode)
{
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0, 6.f), 10.f);
    g.setGradientFill (ColourGradient (Colour (0xfff4f6f7), 0, r.getY(), Colour (0xff8a9095), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.f);

    auto face = r.reduced (9.f);
    Graphics::ScopedSaveState ss (g);
    Path clipP;
    clipP.addRoundedRectangle (face, 5.f);
    g.reduceClipRegion (clipP);

    g.setGradientFill (ColourGradient (Colour (0xfff8edc8), face.getCentreX(), face.getBottom(),
                                       Colour (0xffd4c18a), face.getX(), face.getY(), true));
    g.fillRect (face);

    const float s = face.getWidth() / 200.f, ox = face.getX(), oy = face.getY();
    const float lo = levelMode ? -20.f : 0.f, hi = levelMode ? 3.f : 20.f, redFrom = levelMode ? 0.f : 15.f;
    auto P = [&] (float deg, float rad)
    {
        const float a = degreesToRadians (deg);
        return Point<float> (ox + (100.f + rad * std::sin (a)) * s, oy + (135.f - rad * std::cos (a)) * s);
    };
    auto A = [&] (float v) { return -48.f + (v - lo) / (hi - lo) * 96.f; };

    Path base, red;
    base.addCentredArc (ox + 100.f * s, oy + 135.f * s, 95.f * s, 95.f * s, 0.f, degreesToRadians (A (lo)), degreesToRadians (A (redFrom)), true);
    red.addCentredArc  (ox + 100.f * s, oy + 135.f * s, 95.f * s, 95.f * s, 0.f, degreesToRadians (A (redFrom)), degreesToRadians (A (hi)), true);
    g.setColour (Colour (0xff2a2620));
    g.strokePath (base, PathStrokeType (1.2f * s));
    g.setColour (Colour (0xffb3261e));
    g.strokePath (red, PathStrokeType (3.2f * s));

    for (int v = (int) lo; v <= (int) hi; ++v)
    {
        const bool major = levelMode ? (v == -20 || v == -10 || v == -5 || v == 0 || v == 3) : (v % 5 == 0);
        const bool isRed = (float) v >= redFrom;
        g.setColour (isRed ? Colour (0xffb3261e) : Colour (0xff2a2620));
        g.drawLine (Line<float> (P (A ((float) v), 95.f), P (A ((float) v), major ? 83.f : 89.f)), (major ? 1.6f : 0.8f) * s);
        if (major)
        {
            auto p = P (A ((float) v), 73.f);
            txt (g, (levelMode && v > 0 ? "+" : "") + String (v), R (p.x - 12.f, p.y - 7.f, 24, 14), 9.f * s + 1.f,
                 isRed ? Colour (0xffb3261e) : Colour (0xff2a2620), Justification::centred, false);
        }
    }

    txt (g, "BOOMHYPE TECH", R (ox, oy + 17.f * s, face.getWidth(), 10.f), 6.5f, Colour (0xff4a4436));
    txt (g, levelMode ? "VU" : "dB", R (ox, oy + 104.f * s, face.getWidth(), 12.f), 9.f, Colour (0xff2a2620), Justification::centred, false);

    const float deg = A (jlimit (lo, hi + 1.f, needle));
    const auto pivot = Point<float> (ox + 100.f * s, oy + 135.f * s);
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawLine (Line<float> (pivot.translated (3.f, 3.f), P (deg, 101.f).translated (3.f, 3.f)), 2.f);
    g.setColour (Colour (0xff171513));
    g.drawLine (Line<float> (pivot, P (deg, 101.f)), 1.6f * s);

    g.setGradientFill (ColourGradient (Colour (0x66ffffff), face.getX(), face.getY(), Colour (0x00ffffff), face.getCentreX(), face.getCentreY(), false));
    g.fillRect (face);
}

//==============================================================================
SnobbyEditor::SnobbyEditor (SnobbyAudioProcessor& p)
    : AudioProcessorEditor (&p), face (p)
{
    addAndMakeVisible (face);
    constrainer.setFixedAspectRatio ((double) SnobbyFace::baseW / (double) SnobbyFace::baseH);
    constrainer.setSizeLimits (432, 456, 1440, 1520);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (SnobbyFace::baseW, SnobbyFace::baseH);
}

void SnobbyEditor::resized()
{
    face.setBounds (0, 0, SnobbyFace::baseW, SnobbyFace::baseH);
    face.setTransform (AffineTransform::scale ((float) getWidth() / (float) SnobbyFace::baseW));
}
