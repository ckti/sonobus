/*
  ==============================================================================

    PeersContainerView.cpp
    Created: 27 Jun 2020 12:42:15pm
    Author:  Jesse Chappell

  ==============================================================================
*/

#include "PeersContainerView.h"
#include "DebugLogC.h"
#include "JitterBufferMeter.h"

PeerViewInfo::PeerViewInfo() : smallLnf(12), medLnf(14)
{
    bgColor = Colour::fromFloatRGBA(0.112f, 0.112f, 0.112f, 1.0f);
    borderColor = Colour::fromFloatRGBA(0.5f, 0.5f, 0.5f, 0.3f);

    //Random rcol;
    //itemColor = Colour::fromHSV(rcol.nextFloat(), 0.5f, 0.2f, 1.0f);
}

PeerViewInfo::~PeerViewInfo()
{
    
}

enum {
    LabelTypeRegular = 0,
    LabelTypeSmallDim ,
    LabelTypeSmall
};

enum {
    FillRatioUpdateTimerId = 0
};

void PeerViewInfo::paint(Graphics& g) 
{
    //g.fillAll (Colour(0xff111111));
    //g.fillAll (Colour(0xff202020));
    
    g.setColour(bgColor);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

    g.setColour(borderColor);
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 6.0f, 0.5f);

}

void PeerViewInfo::resized()
{
    
    mainbox.performLayout(getLocalBounds());
    
    //if (levelLabel) {
    //    levelLabel->setBounds(levelSlider->getBounds().removeFromLeft(levelSlider->getWidth()*0.5));
    //}
    
    if (latActiveButton) {
        latActiveButton->setBounds(staticPingLabel->getX(), staticPingLabel->getY(), pingLabel->getRight() - staticPingLabel->getX(), latencyLabel->getBottom() - pingLabel->getY());
    }
    if (menuButton) {
        menuButton->setBounds(staticSendQualLabel->getX(), staticSendQualLabel->getY(), recvActualBitrateLabel->getRight() - staticSendQualLabel->getX(), sendActualBitrateLabel->getBottom() - staticSendQualLabel->getY());
    }

    //Rectangle<int> optbounds(staticSendQualLabel->getX(), staticSendQualLabel->getY(), sendQualityLabel->getRight() - staticSendQualLabel->getX(), bufferLabel->getBottom() - sendQualityLabel->getY());
    //optionsButton->setBounds(optbounds);
    
    if (jitterBufferMeter) {
        jitterBufferMeter->setBounds(bufferLabel->getBounds().reduced(0, 1));
    }
              
    if (statsBg) {
        auto statsbounds = menuButton->getBounds();
        statsBg->setRectangle (statsbounds.toFloat().expanded(1.0f));

        auto pingbounds = latActiveButton->getBounds();
        if (pingbounds.getBottom() < statsbounds.getY()) {
            pingbounds.setBottom(statsbounds.getY() + 10);
        }
        pingBg->setRectangle (pingbounds.toFloat().expanded(1.0f));
    }
    
}


PeersContainerView::PeersContainerView(SonobusAudioProcessor& proc)
 : Component("pcv"),  processor(proc)
{
    mutedTextColor = Colour::fromFloatRGBA(0.8, 0.5, 0.2, 1.0);
    regularTextColor = Colour(0xa0eeeeee);; //Colour(0xc0eeeeee);
    dimTextColor = Colour(0xa0aaaaaa); //Colour(0xc0aaaaaa);
    
    droppedTextColor = Colour(0xc0ee8888);

    outlineColor = Colour::fromFloatRGBA(0.25, 0.25, 0.25, 1.0);
    bgColor = Colours::black;
    
    rebuildPeerViews();
}

void PeersContainerView::configLevelSlider(Slider * slider)
{
    //slider->setTextValueSuffix(" dB");
    slider->setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider->setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider->setColour(Slider::textBoxTextColourId, Colour(0x90eeeeee));
    slider->setColour(TooltipWindow::textColourId, Colour(0xf0eeeeee));

    
    slider->setTextBoxStyle(Slider::TextBoxRight, true, 60, 14);

    slider->setRange(0.0, 2.0, 0.0);
    slider->setSkewFactor(0.25);
    slider->setDoubleClickReturnValue(true, 1.0);
    slider->setTextBoxIsEditable(true);
    slider->setSliderSnapsToMousePosition(false);
    slider->setScrollWheelEnabled(false);
    slider->valueFromTextFunction = [](const String& s) -> float { return Decibels::decibelsToGain(s.getFloatValue()); };
    slider->textFromValueFunction = [](float v) -> String { return Decibels::toString(Decibels::gainToDecibels(v), 1); };
#if JUCE_IOS
    //slider->setPopupDisplayEnabled(true, false, this);
#endif
}


void PeersContainerView::configLabel(Label *label, int ltype)
{
    if (ltype == LabelTypeSmallDim) {
        label->setFont(12);
        label->setColour(Label::textColourId, dimTextColor);
        label->setJustificationType(Justification::centredRight);        
        label->setMinimumHorizontalScale(0.3);
    }
    else if (ltype == LabelTypeSmall) {
        label->setFont(12);
        label->setColour(Label::textColourId, regularTextColor);
        label->setJustificationType(Justification::centredRight);
        label->setMinimumHorizontalScale(0.3);
    }
    else {
        label->setFont(14);
        //label->setColour(Label::textColourId, Colour(0xaaeeeeee));
        label->setJustificationType(Justification::centredLeft);
    }
}





void PeersContainerView::resized()
{
    Rectangle<int> bounds = getLocalBounds().reduced(5, 0);
    bounds.removeFromLeft(3);
    peersBox.performLayout(bounds);
    
    for (int i=0; i < mPeerViews.size(); ++i) {
        PeerViewInfo * pvf = mPeerViews.getUnchecked(i);
        pvf->resized();
    }
}

void PeersContainerView::showPopTip(const String & message, int timeoutMs, Component * target, int maxwidth)
{
    popTip.reset(new BubbleMessageComponent());
    popTip->setAllowedPlacement(BubbleComponent::above);
    
    if (target) {
        target->getTopLevelComponent()->addChildComponent (popTip.get());
    }
    else {
        addChildComponent(popTip.get());
    }
    
    AttributedString text(message);
    text.setJustification (Justification::centred);
    text.setColour (findColour (TextButton::textColourOffId));
    text.setFont(Font(12));
    if (target) {
        popTip->showAt(target, text, timeoutMs);
    }
    else {
        Rectangle<int> topbox(getWidth()/2 - maxwidth/2, 0, maxwidth, 2);
        popTip->showAt(topbox, text, timeoutMs);
    }
}

void PeersContainerView::paint(Graphics & g)
{    
    //g.fillAll (Colours::black);
    Rectangle<int> bounds = getLocalBounds();

    bounds.reduce(1, 1);
    bounds.removeFromLeft(3);
    
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    g.setColour(outlineColor);
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 0.5f);

}

PeerViewInfo * PeersContainerView::createPeerViewInfo()
{
    /*
    struct PeerViewInfo {
        std::unique_ptr<TextButton> activeButton;
        std::unique_ptr<Label>  levelLabel;
        std::unique_ptr<Slider> levelSlider;
        std::unique_ptr<Label>  bufferTimeLabel;
        std::unique_ptr<Slider> bufferTimeSlider;        
    };
*/
    
    PeerViewInfo * pvf = new PeerViewInfo();

    pvf->nameLabel = std::make_unique<Label>("name", "");
    pvf->nameLabel->setJustificationType(Justification::centredLeft);
    pvf->nameLabel->setFont(16);

    pvf->addrLabel = std::make_unique<Label>("name", "");
    pvf->addrLabel->setJustificationType(Justification::centredLeft);
    configLabel(pvf->addrLabel.get(), LabelTypeSmallDim);
    pvf->addrLabel->setFont(13);

    pvf->staticAddrLabel = std::make_unique<Label>("addr", TRANS("Remote address:"));
    pvf->staticAddrLabel->setJustificationType(Justification::centredRight);
    configLabel(pvf->staticAddrLabel.get(), LabelTypeSmallDim);
    pvf->staticAddrLabel->setFont(13);
    
    pvf->sendMutedButton = std::make_unique<SonoDrawableButton>("sendmute", DrawableButton::ButtonStyle::ImageOnButtonBackground);
    std::unique_ptr<Drawable> sendallowimg(Drawable::createFromImageData(BinaryData::mic_svg, BinaryData::mic_svgSize));
    std::unique_ptr<Drawable> senddisallowimg(Drawable::createFromImageData(BinaryData::mic_disabled_svg, BinaryData::mic_disabled_svgSize));
    pvf->sendMutedButton->setImages(sendallowimg.get(), nullptr, nullptr, nullptr, senddisallowimg.get());
    pvf->sendMutedButton->addListener(this);
    pvf->sendMutedButton->setClickingTogglesState(true);
    //pvf->sendMutedButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.15, 0.15, 0.15, 0.7));
    pvf->sendMutedButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    pvf->sendMutedButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    pvf->sendMutedButton->setTooltip(TRANS("Toggles input muting, preventing or allowing your audio to be sent to this user"));

    
    pvf->recvMutedButton = std::make_unique<SonoDrawableButton>("recvmute", DrawableButton::ButtonStyle::ImageOnButtonBackground);
    std::unique_ptr<Drawable> recvallowimg(Drawable::createFromImageData(BinaryData::speaker_svg, BinaryData::speaker_svgSize));
    std::unique_ptr<Drawable> recvdisallowimg(Drawable::createFromImageData(BinaryData::speaker_disabled_svg, BinaryData::speaker_disabled_svgSize));
    pvf->recvMutedButton->setImages(recvallowimg.get(), nullptr, nullptr, nullptr, recvdisallowimg.get());
    pvf->recvMutedButton->addListener(this);
    pvf->recvMutedButton->setClickingTogglesState(true);
    pvf->recvMutedButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    pvf->recvMutedButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    pvf->recvMutedButton->setTooltip(TRANS("Toggles receive muting, preventing or allowing audio to be heard from this user"));

    pvf->latActiveButton = std::make_unique<SonoDrawableButton>("", DrawableButton::ButtonStyle::ImageFitted); // (TRANS("Latency\nTest"));
    pvf->latActiveButton->setColour(SonoTextButton::outlineColourId, Colours::transparentBlack);
    pvf->latActiveButton->setColour(DrawableButton::backgroundOnColourId, Colour::fromFloatRGBA(0.4, 0.2, 0.4, 0.7));
    pvf->latActiveButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    pvf->latActiveButton->setClickingTogglesState(true);
    pvf->latActiveButton->setTriggeredOnMouseDown(true);
    pvf->latActiveButton->setLookAndFeel(&pvf->smallLnf);
    pvf->latActiveButton->addListener(this);
    pvf->latActiveButton->addMouseListener(this, false);

    pvf->statusLabel = std::make_unique<Label>("status", "");
    configLabel(pvf->statusLabel.get(), LabelTypeRegular);
    pvf->statusLabel->setJustificationType(Justification::centredLeft);

    
    pvf->levelSlider     = std::make_unique<Slider>(Slider::LinearHorizontal,  Slider::TextBoxRight);
    pvf->levelSlider->setName("level");
    pvf->levelSlider->addListener(this);

    configLevelSlider(pvf->levelSlider.get());
    //pvf->levelSlider->setLookAndFeel(&pvf->smallLnf);

    //pvf->levelLabel = std::make_unique<Label>("level", TRANS("Level"));
    //configLabel(pvf->levelLabel.get(), LabelTypeRegular);

    
    pvf->pannersContainer = std::make_unique<Component>();
    
    pvf->panSlider1     = std::make_unique<Slider>(Slider::LinearBar,  Slider::TextBoxBelow);
    pvf->panSlider1->setName("pan1");
    pvf->panSlider1->addListener(this);
    pvf->panSlider1->getProperties().set ("fromCentre", true);
    pvf->panSlider1->getProperties().set ("noFill", true);
    pvf->panSlider1->setRange(-1, 1, 0.0f);
    pvf->panSlider1->setDoubleClickReturnValue(true, -1.0);
    pvf->panSlider1->setTextBoxIsEditable(false);
    pvf->panSlider1->setSliderSnapsToMousePosition(false);
    pvf->panSlider1->setScrollWheelEnabled(false);
    pvf->panSlider1->setMouseDragSensitivity(100);
    pvf->panSlider1->textFromValueFunction =  [](double v) -> String { if (fabs(v) < 0.01) return TRANS("C"); return String((int)rint(abs(v*100.0f))) + ((v > 0 ? "% R" : "% L")) ; };
    pvf->panSlider1->valueFromTextFunction =  [](const String& s) -> double { return s.getDoubleValue()*1e-2f; };
    pvf->panSlider1->setValue(0.1, dontSendNotification);
    pvf->panSlider1->setValue(0.0, dontSendNotification);

    
    pvf->panSlider2     = std::make_unique<Slider>(Slider::LinearBar,  Slider::TextBoxBelow);
    pvf->panSlider2->setName("pan2");
    pvf->panSlider2->addListener(this);
    pvf->panSlider2->getProperties().set ("fromCentre", true);
    pvf->panSlider2->getProperties().set ("noFill", true);
    pvf->panSlider2->setRange(-1, 1, 0.01f);
    pvf->panSlider2->setDoubleClickReturnValue(true, -1.0);
    pvf->panSlider2->setTextBoxIsEditable(false);
    pvf->panSlider2->setSliderSnapsToMousePosition(false);
    pvf->panSlider2->setScrollWheelEnabled(false);
    pvf->panSlider2->setMouseDragSensitivity(100);

#if JUCE_IOS
    //pvf->panSlider1->setPopupDisplayEnabled(true, false, pvf->pannersContainer.get());
    //pvf->panSlider2->setPopupDisplayEnabled(true, false, pvf->pannersContainer.get());
#endif
    
    pvf->panSlider2->textFromValueFunction =  [](double v) -> String { if ( fabs(v) < 0.01) return TRANS("C"); return String((int)rint(abs(v*100.0f))) + ((v > 0 ? "% R" : "% L")) ; };
    pvf->panSlider2->valueFromTextFunction =  [](const String& s) -> double { return s.getDoubleValue()*1e-2f; };

    pvf->panButton = std::make_unique<TextButton>(TRANS("Pan"));
    pvf->panButton->addListener(this);
    pvf->panButton->setLookAndFeel(&pvf->medLnf);

    //pvf->optionsButton = std::make_unique<TextButton>("");
    //pvf->optionsButton->addListener(this);

    pvf->optionsButton = std::make_unique<SonoDrawableButton>("settings",  DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> setimg(Drawable::createFromImageData(BinaryData::settings_icon_svg, BinaryData::settings_icon_svgSize));
    pvf->optionsButton->setImages(setimg.get());
    pvf->optionsButton->addListener(this);
    pvf->optionsButton->setAlpha(0.7);
   // pvf->optionsButton->addMouseListener(this, false);

    
    
    pvf->bufferTimeSlider     = std::make_unique<Slider>(Slider::LinearBar,  Slider::TextBoxBelow);
    pvf->bufferTimeSlider->setName("buffer");
    pvf->bufferTimeSlider->setRange(0, 1000, 1);
    pvf->bufferTimeSlider->setTextValueSuffix(" ms");
    pvf->bufferTimeSlider->getProperties().set ("noFill", true);
    pvf->bufferTimeSlider->setSkewFactor(0.4);
    pvf->bufferTimeSlider->setDoubleClickReturnValue(true, 20.0);
    pvf->bufferTimeSlider->setTextBoxIsEditable(false);
    pvf->bufferTimeSlider->setSliderSnapsToMousePosition(false);
    pvf->bufferTimeSlider->setChangeNotificationOnlyOnRelease(true);
    pvf->bufferTimeSlider->setScrollWheelEnabled(false);
    pvf->bufferTimeSlider->setPopupDisplayEnabled(true, false, this);

    pvf->bufferTimeSlider->addListener(this);
    //configKnobSlider(pvf->bufferTimeSlider.get());

    pvf->autosizeButton = std::make_unique<SonoChoiceButton>();
    pvf->autosizeButton->addChoiceListener(this);
    pvf->autosizeButton->addItem(TRANS("Manual"), 1);
    pvf->autosizeButton->addItem(TRANS("Auto Up"), 1);
    pvf->autosizeButton->addItem(TRANS("Auto"), 1);
    pvf->autosizeButton->addListener(this);


    
    pvf->bufferTimeLabel = std::make_unique<Label>("level", TRANS("Safety Buffer"));
    configLabel(pvf->bufferTimeLabel.get(), LabelTypeRegular);

    pvf->menuButton = std::make_unique<SonoDrawableButton>("menu", DrawableButton::ImageFitted);
    //DrawableImage dotimg;
    //dotimg.setImage(ImageCache::getFromMemory (BinaryData::dots_icon_png, BinaryData::dots_icon_pngSize));
    //pvf->menuButton->setImages(&dotimg);
    pvf->menuButton->addListener(this);
    pvf->menuButton->setColour(SonoTextButton::outlineColourId, Colours::transparentBlack);
    pvf->menuButton->setColour(DrawableButton::backgroundOnColourId, Colour::fromFloatRGBA(0.4, 0.2, 0.4, 0.7));
    pvf->menuButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);

    pvf->formatChoiceButton = std::make_unique<SonoChoiceButton>();
    pvf->formatChoiceButton->addChoiceListener(this);
    int numformats = processor.getNumberAudioCodecFormats();
    for (int i=0; i < numformats; ++i) {
        pvf->formatChoiceButton->addItem(processor.getAudioCodeFormatName(i), i+1);
    }

    pvf->staticFormatChoiceLabel = std::make_unique<Label>("fmt", TRANS("Send Quality"));
    configLabel(pvf->staticFormatChoiceLabel.get(), LabelTypeRegular);

    
    pvf->staticLatencyLabel = std::make_unique<Label>("lat", TRANS("Latency:"));
    configLabel(pvf->staticLatencyLabel.get(), LabelTypeSmallDim);
    pvf->staticPingLabel = std::make_unique<Label>("ping", TRANS("Ping:"));
    configLabel(pvf->staticPingLabel.get(), LabelTypeSmallDim);

    pvf->latencyLabel = std::make_unique<Label>("lat", TRANS("PRESS"));
    configLabel(pvf->latencyLabel.get(), LabelTypeSmall);
    pvf->pingLabel = std::make_unique<Label>("ping");
    configLabel(pvf->pingLabel.get(), LabelTypeSmall);

    pvf->staticSendQualLabel = std::make_unique<Label>("lat", TRANS("Send Quality:"));
    configLabel(pvf->staticSendQualLabel.get(), LabelTypeSmallDim);
    pvf->staticBufferLabel = std::make_unique<Label>("ping", TRANS("Safety Buffer:"));
    configLabel(pvf->staticBufferLabel.get(), LabelTypeSmallDim);
    
    pvf->sendQualityLabel = std::make_unique<Label>("qual", "");
    configLabel(pvf->sendQualityLabel.get(), LabelTypeSmall);
    pvf->sendQualityLabel->setJustificationType(Justification::centredLeft);

    pvf->bufferLabel = std::make_unique<Label>("buf");
    configLabel(pvf->bufferLabel.get(), LabelTypeSmall);
    pvf->bufferLabel->setJustificationType(Justification::centredLeft);

    
    pvf->sendActualBitrateLabel = std::make_unique<Label>("sbit");
    configLabel(pvf->sendActualBitrateLabel.get(), LabelTypeSmall);
    pvf->sendActualBitrateLabel->setJustificationType(Justification::centred);
    pvf->sendActualBitrateLabel->setMinimumHorizontalScale(0.75);

    pvf->recvActualBitrateLabel = std::make_unique<Label>("rbit");
    configLabel(pvf->recvActualBitrateLabel.get(), LabelTypeSmall);
    //pvf->recvActualBitrateLabel->addMouseListener(this, false);
    //pvf->recvActualBitrateLabel->setInterceptsMouseClicks(true, false);
    pvf->recvActualBitrateLabel->setJustificationType(Justification::centred);
    pvf->recvActualBitrateLabel->setMinimumHorizontalScale(0.75);
    
    
    pvf->jitterBufferMeter = std::make_unique<JitterBufferMeter>();
    
    pvf->optionsResetDropButton = std::make_unique<TextButton>(TRANS("Reset Dropped"));
    //pvf->optionsResetDropButton->setColour(TextButton::buttonColourId, Colours::black);
    pvf->optionsResetDropButton->addListener(this);
    pvf->optionsResetDropButton->setLookAndFeel(&pvf->medLnf);

    pvf->optionsRemoveButton = std::make_unique<TextButton>(TRANS("Remove"));
    pvf->optionsRemoveButton->addListener(this);
    pvf->optionsRemoveButton->setLookAndFeel(&pvf->medLnf);

    
    // meters
    auto flags = foleys::LevelMeter::Horizontal|foleys::LevelMeter::Minimal;
    
    pvf->recvMeter = std::make_unique<foleys::LevelMeter>(foleys::LevelMeter::Minimal);
    pvf->recvMeter->setLookAndFeel(&(pvf->rmeterLnf));
    pvf->recvMeter->setRefreshRateHz(10);
    pvf->recvMeter->addMouseListener(this, false);

    pvf->optionsContainer = std::make_unique<Component>();

    Colour statsbgcol = Colour::fromFloatRGBA(0.07, 0.07, 0.07, 1.0); // 0.08
    Colour statsbordcol = Colour::fromFloatRGBA(0.5, 0.5, 0.5, 0.0); // 0.25
    pvf->statsBg = std::make_unique<DrawableRectangle>();
    pvf->statsBg->setCornerSize(Point<float>(6,6));
    pvf->statsBg->setFill (statsbgcol);
    pvf->statsBg->setStrokeFill (statsbordcol);
    pvf->statsBg->setStrokeThickness(0.5);

    pvf->pingBg = std::make_unique<DrawableRectangle>();
    pvf->pingBg->setCornerSize(Point<float>(6,6));
    pvf->pingBg->setFill (statsbgcol);
    pvf->pingBg->setStrokeFill (statsbordcol);
    pvf->pingBg->setStrokeThickness(0.5);

    
    //pvf->sendMeter = std::make_unique<foleys::LevelMeter>(flags);
    //pvf->sendMeter->setLookAndFeel(&(pvf->smeterLnf));
    
    return pvf;
}
    


void PeersContainerView::rebuildPeerViews()
{
    int numpeers = processor.getNumberRemotePeers();
    
    showOptions(0, false);
    
    while (mPeerViews.size() < numpeers) {
        mPeerViews.add(createPeerViewInfo());        
    }
    while (mPeerViews.size() > numpeers) {
        mPeerViews.removeLast();
    }
    
    for (int i=0; i < mPeerViews.size(); ++i) {
        PeerViewInfo * pvf = mPeerViews.getUnchecked(i);

        pvf->addAndMakeVisible(pvf->statsBg.get());
        pvf->addAndMakeVisible(pvf->pingBg.get());
        pvf->addAndMakeVisible(pvf->sendMutedButton.get());
        pvf->addAndMakeVisible(pvf->recvMutedButton.get());
        pvf->addAndMakeVisible(pvf->latActiveButton.get());
        pvf->addAndMakeVisible(pvf->levelSlider.get());
        //pvf->addAndMakeVisible(pvf->levelLabel.get());
        pvf->addAndMakeVisible(pvf->menuButton.get());
        pvf->addAndMakeVisible(pvf->staticLatencyLabel.get());
        pvf->addAndMakeVisible(pvf->staticPingLabel.get());
        pvf->addAndMakeVisible(pvf->latencyLabel.get());
        pvf->addAndMakeVisible(pvf->pingLabel.get());
        pvf->addAndMakeVisible(pvf->nameLabel.get());
        pvf->addAndMakeVisible(pvf->statusLabel.get());
        pvf->addAndMakeVisible(pvf->jitterBufferMeter.get());
        //pvf->addAndMakeVisible(pvf->optionsButton.get());
        pvf->addAndMakeVisible(pvf->staticSendQualLabel.get());
        pvf->addAndMakeVisible(pvf->staticBufferLabel.get());
        pvf->addAndMakeVisible(pvf->sendQualityLabel.get());
        pvf->addAndMakeVisible(pvf->bufferLabel.get());

        pvf->optionsContainer->addAndMakeVisible(pvf->addrLabel.get());
        pvf->optionsContainer->addAndMakeVisible(pvf->staticAddrLabel.get());
        pvf->optionsContainer->addAndMakeVisible(pvf->autosizeButton.get());
        pvf->optionsContainer->addAndMakeVisible(pvf->formatChoiceButton.get());
        pvf->optionsContainer->addAndMakeVisible(pvf->staticFormatChoiceLabel.get());
        pvf->optionsContainer->addAndMakeVisible(pvf->bufferTimeSlider.get());
        pvf->optionsContainer->addAndMakeVisible(pvf->bufferTimeLabel.get());
        pvf->optionsContainer->addAndMakeVisible(pvf->optionsRemoveButton.get());
        pvf->optionsContainer->addAndMakeVisible(pvf->optionsResetDropButton.get());

        //pvf->addAndMakeVisible(pvf->sendMeter.get());
        pvf->addAndMakeVisible(pvf->recvMeter.get());
        pvf->addAndMakeVisible(pvf->sendActualBitrateLabel.get());
        pvf->addAndMakeVisible(pvf->recvActualBitrateLabel.get());
        pvf->addAndMakeVisible(pvf->panButton.get());

        pvf->pannersContainer->addAndMakeVisible(pvf->panSlider1.get());
        pvf->pannersContainer->addAndMakeVisible(pvf->panSlider2.get());

        addAndMakeVisible(pvf);
    }
    
    if (mPeerViews.size() > 0) {
        startTimer(FillRatioUpdateTimerId, 100);
    } else {
        stopTimer(FillRatioUpdateTimerId);
    }
    
    updatePeerViews();
    updateLayout();
    resized();
}

void PeersContainerView::updateLayout()
{
    int minitemheight = 36;
    int mincheckheight = 32;
    int minPannerWidth = 40;
    int minButtonWidth = 90;
    
    int mutebuttwidth = 60;
    
#if JUCE_IOS
    // make the button heights a bit more for touchscreen purposes
    minitemheight = 44;
    mincheckheight = 40;
    
#endif

    const int textheight = minitemheight / 2;

    
    peersBox.items.clear();
    peersBox.flexDirection = FlexBox::Direction::column;
    peersBox.justifyContent = FlexBox::JustifyContent::flexStart;
    int peersheight = 0;
    //const int singleph =  minitemheight*3 + 12;
    const int singleph =  minitemheight*2 + 6;
    
    peersBox.items.add(FlexItem(8, 2).withMargin(0));
    peersheight += 2;

    
    for (int i=0; i < mPeerViews.size(); ++i) {
        PeerViewInfo * pvf = mPeerViews.getUnchecked(i);

        //pvf->updateLayout();
        
        const int ph = singleph;
        const int pw = 200;
        


        pvf->nameaddrbox.items.clear();
        pvf->nameaddrbox.flexDirection = FlexBox::Direction::column;
        pvf->nameaddrbox.items.add(FlexItem(100, 18, *pvf->nameLabel).withMargin(0).withFlex(1));
        //pvf->nameaddrbox.items.add(FlexItem(100, 14, *pvf->addrLabel).withMargin(0).withFlex(0));
        pvf->nameaddrbox.items.add(FlexItem(3, 4));

        pvf->namebox.items.clear();
        pvf->namebox.flexDirection = FlexBox::Direction::row;
//.withAlignSelf(FlexItem::AlignSelf::center));
        pvf->namebox.items.add(FlexItem(100, minitemheight, pvf->nameaddrbox).withMargin(2).withFlex(1));

        
       
        
        pvf->sendmeterbox.items.clear();
        pvf->sendmeterbox.flexDirection = FlexBox::Direction::row;
        //pvf->sendmeterbox.items.add(FlexItem(60, minitemheight, *pvf->sendMeter).withMargin(0).withFlex(1));
        //if (!isNarrow) {
        //    pvf->sendmeterbox.items.add(FlexItem(60, minitemheight, *pvf->formatChoiceButton).withMargin(0).withFlex(2));
        //    pvf->sendmeterbox.items.add(FlexItem(60, minitemheight, *pvf->sendActualBitrateLabel).withMargin(0).withFlex(1));
        //}

      
        
        pvf->pingbox.items.clear();
        pvf->pingbox.flexDirection = FlexBox::Direction::row;
        pvf->pingbox.items.add(FlexItem(40, textheight, *pvf->staticPingLabel).withMargin(0).withFlex(0.5));
        pvf->pingbox.items.add(FlexItem(20, textheight, *pvf->pingLabel).withMargin(0).withFlex(0.5));

        pvf->latencybox.items.clear();
        pvf->latencybox.flexDirection = FlexBox::Direction::row;
        pvf->latencybox.items.add(FlexItem(40, textheight, *pvf->staticLatencyLabel).withMargin(0).withFlex(0.5));
        pvf->latencybox.items.add(FlexItem(20, textheight, *pvf->latencyLabel).withMargin(0).withFlex(0.5));

        
        pvf->netstatbox.items.clear();
        pvf->netstatbox.flexDirection = FlexBox::Direction::column;
        pvf->netstatbox.items.add(FlexItem(80, textheight, pvf->pingbox).withMargin(0).withFlex(0));
        pvf->netstatbox.items.add(FlexItem(80, textheight, pvf->latencybox).withMargin(0).withFlex(0));

        
        pvf->squalbox.items.clear();
        pvf->squalbox.flexDirection = FlexBox::Direction::row;
        pvf->squalbox.items.add(FlexItem(60, textheight, *pvf->staticSendQualLabel).withMargin(0).withFlex(1));
        pvf->squalbox.items.add(FlexItem(40, textheight, *pvf->sendQualityLabel).withMargin(0).withFlex(2));

        pvf->netbufbox.items.clear();
        pvf->netbufbox.flexDirection = FlexBox::Direction::row;
        pvf->netbufbox.items.add(FlexItem(0, 10).withFlex(1));
        pvf->netbufbox.items.add(FlexItem(80, textheight, *pvf->staticBufferLabel).withMargin(0).withFlex(0));
        pvf->netbufbox.items.add(FlexItem(40, textheight, *pvf->bufferLabel).withMargin(0).withFlex(3).withMaxWidth(120));
        pvf->netbufbox.items.add(FlexItem(0, 10).withFlex(1));

        
        pvf->optionsstatbox.items.clear();
        pvf->optionsstatbox.flexDirection = FlexBox::Direction::column;
        pvf->optionsstatbox.items.add(FlexItem(100, textheight, pvf->squalbox).withMargin(0).withFlex(0));
        pvf->optionsstatbox.items.add(FlexItem(76, textheight, *pvf->sendActualBitrateLabel).withMargin(0).withFlex(0));

        
        pvf->recvstatbox.items.clear();
        pvf->recvstatbox.flexDirection = FlexBox::Direction::column;
        pvf->recvstatbox.items.add(FlexItem(100, textheight, pvf->netbufbox).withMargin(0).withFlex(0));
        pvf->recvstatbox.items.add(FlexItem(100, textheight, *pvf->recvActualBitrateLabel).withMargin(0).withFlex(0));

        
        // options
        pvf->optionsNetbufBox.items.clear();
        pvf->optionsNetbufBox.flexDirection = FlexBox::Direction::row;
        //optionsNetbufBox.items.add(FlexItem(minButtonWidth, minitemheight, *mOptionsAutosizeStaticLabel).withMargin(0).withFlex(1));
        //pvf->optionsNetbufBox.items.add(FlexItem(12, 12));
        pvf->optionsNetbufBox.items.add(FlexItem(minButtonWidth, minitemheight, *pvf->bufferTimeSlider).withMargin(0).withFlex(1));
        pvf->optionsNetbufBox.items.add(FlexItem(4, 12));
        pvf->optionsNetbufBox.items.add(FlexItem(80, minitemheight, *pvf->autosizeButton).withMargin(0).withFlex(0));
        
        pvf->optionsSendQualBox.items.clear();
        pvf->optionsSendQualBox.flexDirection = FlexBox::Direction::row;
        pvf->optionsSendQualBox.items.add(FlexItem(100, minitemheight, *pvf->staticFormatChoiceLabel).withMargin(0).withFlex(0));
        pvf->optionsSendQualBox.items.add(FlexItem(minButtonWidth, minitemheight, *pvf->formatChoiceButton).withMargin(0).withFlex(2));

        pvf->optionsbuttbox.items.clear();
        pvf->optionsbuttbox.flexDirection = FlexBox::Direction::row;
        pvf->optionsbuttbox.items.add(FlexItem(12, 12).withFlex(1));
        pvf->optionsbuttbox.items.add(FlexItem(100, minitemheight, *pvf->optionsResetDropButton).withMargin(0).withFlex(0));
        pvf->optionsbuttbox.items.add(FlexItem(12, 12).withFlex(1));
        pvf->optionsbuttbox.items.add(FlexItem(minButtonWidth, minitemheight, *pvf->optionsRemoveButton).withMargin(0).withFlex(0));
        pvf->optionsbuttbox.items.add(FlexItem(12, 12).withFlex(1));
        
        pvf->optionsaddrbox.items.clear();
        pvf->optionsaddrbox.flexDirection = FlexBox::Direction::row;
        //pvf->optionsbuttbox.items.add(FlexItem(12, 12).withFlex(1));
        pvf->optionsaddrbox.items.add(FlexItem(120, 14, *pvf->staticAddrLabel).withMargin(0).withFlex(1));
        pvf->optionsaddrbox.items.add(FlexItem(100, 14, *pvf->addrLabel).withMargin(0).withFlex(2));

        
        pvf->optionsBox.items.clear();
        pvf->optionsBox.flexDirection = FlexBox::Direction::column;
        pvf->optionsBox.items.add(FlexItem(4, 4));
        pvf->optionsBox.items.add(FlexItem(100, 14,  pvf->optionsaddrbox).withMargin(2).withFlex(0));
        pvf->optionsBox.items.add(FlexItem(4, 4));
        pvf->optionsBox.items.add(FlexItem(100, minitemheight,  pvf->optionsSendQualBox).withMargin(2).withFlex(0));
        pvf->optionsBox.items.add(FlexItem(4, 2));
        pvf->optionsBox.items.add(FlexItem(100, minitemheight,  pvf->optionsNetbufBox).withMargin(2).withFlex(0));
        pvf->optionsBox.items.add(FlexItem(4, 8));
        pvf->optionsBox.items.add(FlexItem(100, minitemheight,  pvf->optionsbuttbox).withMargin(2).withFlex(0));
        
        /*
        pvf->recvnetbox.items.clear();
        pvf->recvnetbox.flexDirection = FlexBox::Direction::row;
        pvf->recvnetbox.items.add(FlexItem(72, minitemheight, *pvf->autosizeButton).withMargin(0).withFlex(0));
        pvf->recvnetbox.items.add(FlexItem(2, 5));
        pvf->recvnetbox.items.add(FlexItem(60, minitemheight, *pvf->bufferTimeSlider).withMargin(0).withFlex(1));
        pvf->recvnetbox.items.add(FlexItem(2, 5));
        //pvf->recvnetbox.items.add(FlexItem(100, minitemheight, pvf->netstatbox).withMargin(0).withFlex(0));
         */
         
        pvf->pannerbox.items.clear();
        pvf->pannerbox.flexDirection = FlexBox::Direction::row;
        
        pvf->sendbox.items.clear();
        pvf->sendbox.flexDirection = FlexBox::Direction::row;
        pvf->sendbox.items.add(FlexItem(5, 2).withFlex(0));
        if (!isNarrow) {
            //pvf->sendbox.items.add(FlexItem(mincheckheight, mincheckheight, *pvf->optionsButton).withMargin(0).withFlex(0).withMaxHeight(minitemheight));
           // pvf->sendbox.items.add(FlexItem(3, 2).withFlex(1));
           // pvf->sendbox.items.add(FlexItem(100, minitemheight, pvf->netstatbox).withMargin(0).withFlex(0).withMaxWidth(200));
           // pvf->sendbox.items.add(FlexItem(3, 2).withFlex(1));
        } else {
            pvf->sendbox.items.add(FlexItem(3, 2).withFlex(0.5));
            pvf->sendbox.items.add(FlexItem(mutebuttwidth, minitemheight, *pvf->sendMutedButton).withMargin(0).withFlex(0));
            pvf->sendbox.items.add(FlexItem(5, 2));
            pvf->sendbox.items.add(FlexItem(mutebuttwidth, mincheckheight, *pvf->recvMutedButton).withMargin(0).withFlex(0));
            pvf->sendbox.items.add(FlexItem(3, 5).withFlex(0.5));
            pvf->sendbox.items.add(FlexItem(90, minitemheight, pvf->netstatbox).withMargin(0).withFlex(1).withMaxWidth(130));
            pvf->sendbox.items.add(FlexItem(3, 5));
        }
        
        pvf->recvbox.items.clear();
        pvf->recvbox.flexDirection = FlexBox::Direction::row;
        if (isNarrow) {
            //pvf->recvbox.items.add(FlexItem(mincheckheight, mincheckheight, *pvf->optionsButton).withMargin(0).withFlex(0).withMaxHeight(minitemheight));
            //pvf->recvbox.items.add(FlexItem(3, 5));
        }
        pvf->recvbox.items.add(FlexItem(1, 5));
        pvf->recvbox.items.add(FlexItem(60, minitemheight, pvf->optionsstatbox).withMargin(0).withFlex(1).withMaxWidth(300));
        pvf->recvbox.items.add(FlexItem(3, 5));
        pvf->recvbox.items.add(FlexItem(100, minitemheight, pvf->recvstatbox).withMargin(0).withFlex(1));
        pvf->recvbox.items.add(FlexItem(3, 5));
        if ( ! isNarrow) {            
            pvf->recvbox.items.add(FlexItem(2, 2).withFlex(0));
            pvf->recvbox.items.add(FlexItem(100, minitemheight, pvf->netstatbox).withMargin(0).withFlex(1).withMaxWidth(130));
            pvf->recvbox.items.add(FlexItem(3, 2).withFlex(0));
        }
        
        pvf->recvlevelbox.items.clear();
        pvf->recvlevelbox.flexDirection = FlexBox::Direction::row;

        if (!isNarrow) {
            pvf->recvlevelbox.items.add(FlexItem(mutebuttwidth, minitemheight, *pvf->sendMutedButton).withMargin(0).withFlex(0));
            pvf->recvlevelbox.items.add(FlexItem(5, 2));
            pvf->recvlevelbox.items.add(FlexItem(mutebuttwidth, mincheckheight, *pvf->recvMutedButton).withMargin(0).withFlex(0));
            pvf->recvlevelbox.items.add(FlexItem(3, 5));

            pvf->levelSlider->setTextBoxStyle(Slider::TextBoxRight, true, 60, 14);
            pvf->levelSlider->setPopupDisplayEnabled(false, false, getTopLevelComponent());
        }
        else {
            pvf->levelSlider->setTextBoxStyle(Slider::NoTextBox, true, 50, 10);
            pvf->levelSlider->setPopupDisplayEnabled(true, true, getTopLevelComponent());
        }
        pvf->recvlevelbox.items.add(FlexItem(80, minitemheight, *pvf->levelSlider).withMargin(0).withFlex(2));

        if (processor.getTotalNumOutputChannels() > 1) {

            pvf->recvlevelbox.items.add(FlexItem(3, 5));
            pvf->recvlevelbox.items.add(FlexItem(40, minitemheight, *pvf->panButton).withMargin(0).withFlex(1).withMaxWidth(50));

            pvf->pannerbox.items.add(FlexItem(minPannerWidth, minitemheight, *pvf->panSlider1).withMargin(0).withFlex(1)); //.withMaxWidth(maxPannerWidth));

            if (processor.getRemotePeerChannelCount(i) > 1) {
                pvf->pannerbox.items.add(FlexItem(2, 5));
                pvf->pannerbox.items.add(FlexItem(minPannerWidth, minitemheight, *pvf->panSlider2).withMargin(0).withFlex(1)); //.withMaxWidth(maxPannerWidth));
            }
        }

        pvf->recvlevelbox.items.add(FlexItem(3, 5));

        /*
        pvf->mainrecvbox.items.clear();
        pvf->mainrecvbox.flexDirection = FlexBox::Direction::column;
        pvf->mainrecvbox.items.add(FlexItem(2, 3));
        if (isNarrow) {
        } else {
            pvf->mainrecvbox.items.add(FlexItem(150, minitemheight, pvf->recvlevelbox).withMargin(0).withFlex(0));
        }
        pvf->mainrecvbox.items.add(FlexItem(2, 4));
        pvf->mainrecvbox.items.add(FlexItem(150, minitemheight, pvf->recvbox).withMargin(0).withFlex(0));
        //pvf->mainrecvbox.items.add(FlexItem(150, minitemheight, pvf->recvnetbox).withMargin(2).withFlex(0));
         */
        

        if (isNarrow) {
            pvf->mainsendbox.items.clear();
            pvf->mainsendbox.flexDirection = FlexBox::Direction::row;
            pvf->mainsendbox.items.add(FlexItem(110, minitemheight, pvf->namebox).withMargin(0).withFlex(0));
            pvf->mainsendbox.items.add(FlexItem(3, 3));
            pvf->mainsendbox.items.add(FlexItem(100 + mincheckheight, minitemheight, pvf->recvlevelbox).withMargin(0).withFlex(1));
        } else {
            pvf->mainsendbox.items.clear();
            pvf->mainsendbox.flexDirection = FlexBox::Direction::row;
            pvf->mainsendbox.items.add(FlexItem(110, minitemheight, pvf->namebox).withMargin(0).withFlex(1).withMaxWidth(160));
            pvf->mainsendbox.items.add(FlexItem(3, 3));
            pvf->mainsendbox.items.add(FlexItem(100 + mincheckheight, minitemheight, pvf->recvlevelbox).withMargin(0).withFlex(1));

            //pvf->mainsendbox.items.clear();
            //pvf->mainsendbox.flexDirection = FlexBox::Direction::column;
            //pvf->mainsendbox.items.add(FlexItem(2, 3));
            //pvf->mainsendbox.items.add(FlexItem(110, minitemheight, pvf->namebox).withMargin(0).withFlex(0));
                // pvf->mainsendbox.items.add(FlexItem(2, 4));
                // pvf->mainsendbox.items.add(FlexItem(120, minitemheight, pvf->sendbox).withMargin(0).withFlex(0));
        }
        
        
        pvf->mainbox.items.clear();
        pvf->mainnarrowbox.items.clear();

        if (isNarrow) {
            pvf->mainnarrowbox.flexDirection = FlexBox::Direction::column;
            pvf->mainnarrowbox.items.add(FlexItem(2, 3));
            pvf->mainnarrowbox.items.add(FlexItem(100, minitemheight, pvf->mainsendbox).withMargin(0).withFlex(0));
            pvf->mainnarrowbox.items.add(FlexItem(2, 4));
            pvf->mainnarrowbox.items.add(FlexItem(120, minitemheight, pvf->sendbox).withMargin(0).withFlex(0));
            pvf->mainnarrowbox.items.add(FlexItem(2, 4));
            pvf->mainnarrowbox.items.add(FlexItem(150, minitemheight, pvf->recvbox).withMargin(0).withFlex(0));

            pvf->mainbox.flexDirection = FlexBox::Direction::row;
            pvf->mainbox.items.add(FlexItem(3, 2));
            pvf->mainbox.items.add(FlexItem(150, singleph*2 - minitemheight , pvf->mainnarrowbox).withMargin(0).withFlex(1));
            pvf->mainbox.items.add(FlexItem(2, 2));
            pvf->mainbox.items.add(FlexItem(22, 50, *pvf->recvMeter).withMargin(3).withFlex(0));
        } else {
            pvf->mainnarrowbox.flexDirection = FlexBox::Direction::column;
            pvf->mainnarrowbox.items.add(FlexItem(2, 3));
            pvf->mainnarrowbox.items.add(FlexItem(100, minitemheight, pvf->mainsendbox).withMargin(0).withFlex(0));
            pvf->mainnarrowbox.items.add(FlexItem(2, 4));
            pvf->mainnarrowbox.items.add(FlexItem(150, minitemheight, pvf->recvbox).withMargin(0).withFlex(0));

            pvf->mainbox.flexDirection = FlexBox::Direction::row;
            pvf->mainbox.items.add(FlexItem(3, 2));
            pvf->mainbox.items.add(FlexItem(150, singleph, pvf->mainnarrowbox).withMargin(0).withFlex(1));
            pvf->mainbox.items.add(FlexItem(2, 2));
            pvf->mainbox.items.add(FlexItem(22, 50, *pvf->recvMeter).withMargin(3).withFlex(0));
            
            //pvf->mainbox.flexDirection = FlexBox::Direction::row;
            //pvf->mainbox.items.add(FlexItem(2, 5));
            //pvf->mainbox.items.add(FlexItem(100, singleph, pvf->mainsendbox).withMargin(0).withFlex(1).withMaxWidth(200));
            //pvf->mainbox.items.add(FlexItem(3, 5));
            //pvf->mainbox.items.add(FlexItem(150, singleph, pvf->mainrecvbox).withMargin(0).withFlex(3));
            //pvf->mainbox.items.add(FlexItem(1, 5));
            //pvf->mainbox.items.add(FlexItem(22, 50, *pvf->recvMeter).withMargin(4).withFlex(0));
        }

        peersBox.items.add(FlexItem(8, 4).withMargin(0));
        peersheight += 4;
        
        if (isNarrow) {
            peersBox.items.add(FlexItem(pw, ph*2 - minitemheight + 2, *pvf).withMargin(0).withFlex(0));
            peersheight += ph*2-minitemheight + 2;
        } else {
            peersBox.items.add(FlexItem(pw, ph + 5, *pvf).withMargin(1).withFlex(0));
            peersheight += ph + 5;
        }

    }


    if (isNarrow) {
        peersMinHeight = std::max(singleph*2 + 14,  peersheight);
        peersMinWidth = 300;
    }
    else {
        peersMinHeight = std::max(singleph + 11,  peersheight);
        peersMinWidth = 180 + 150 + mincheckheight + 50;
    }
    
}

Rectangle<int> PeersContainerView::getMinimumContentBounds() const
{
    return Rectangle<int>(0,0,peersMinWidth, peersMinHeight);
}


void PeersContainerView::updatePeerViews()
{
    uint32 nowstampms = Time::getMillisecondCounter();
    
    for (int i=0; i < mPeerViews.size(); ++i) {
        PeerViewInfo * pvf = mPeerViews.getUnchecked(i);
        String hostname;
        int port = 0;
        processor.getRemotePeerAddressInfo(i, hostname, port);
        bool connected = processor.getRemotePeerConnected(i);

        String username = processor.getRemotePeerUserName(i);
        String addrname;
        addrname << hostname << " : " << port;
        
        if (username.isNotEmpty()) {
            pvf->nameLabel->setText(username, dontSendNotification);
            pvf->addrLabel->setText(addrname, dontSendNotification);
            //pvf->addrLabel->setVisible(true);
        } else {
            //pvf->addrLabel->setVisible(false);
            pvf->nameLabel->setText(addrname, dontSendNotification);
        }

        String sendtext;
        bool sendactive = processor.getRemotePeerSendActive(i);
        bool sendallow = processor.getRemotePeerSendAllow(i);

        bool recvactive = processor.getRemotePeerRecvActive(i);
        bool recvallow = processor.getRemotePeerRecvAllow(i);
        bool latactive = processor.isRemotePeerLatencyTestActive(i);

        
        double sendrate = 0.0;
        double recvrate = 0.0;
        
        if (lastUpdateTimestampMs > 0) {
            double timedelta = (nowstampms - lastUpdateTimestampMs) * 1e-3;
            int64_t nbs = processor.getRemotePeerBytesSent(i);
            int64_t nbr = processor.getRemotePeerBytesReceived(i);
            
            sendrate = (nbs - pvf->lastBytesSent) / timedelta;
            recvrate = (nbr - pvf->lastBytesRecv) / timedelta;
            
            pvf->lastBytesRecv = nbr;
            pvf->lastBytesSent = nbs;
        }
                
        if (sendactive) {
            //sendtext += String::formatted("%Ld sent", processor.getRemotePeerPacketsSent(i) );
            sendtext += String::formatted("%.d kb/s", lrintf(sendrate * 8 * 1e-3) );
            pvf->sendActualBitrateLabel->setColour(Label::textColourId, regularTextColor);
        }
        else if (sendallow) {
            sendtext += TRANS("Other end muted us");
            pvf->sendActualBitrateLabel->setColour(Label::textColourId, mutedTextColor);
        }
        else {
            sendtext += TRANS("Self-muted");
            pvf->sendActualBitrateLabel->setColour(Label::textColourId, mutedTextColor);
        }
        
        String recvtext;
        SonobusAudioProcessor::AudioCodecFormatInfo recvfinfo;
        processor.getRemotePeerReceiveAudioCodecFormat(i, recvfinfo);

        if (recvactive) {
            //String recvfmtstr = recvformatindex >= 0 ?processor.getAudioCodeFormatName(recvformatindex) : "";
            //recvtext += String::formatted("%Ld recvd", processor.getRemotePeerPacketsReceived(i));
            //recvtext += String::formatted("%s | %d kb/s | %d%%", recvfinfo.name.toRawUTF8(), lrintf(recvrate * 8 * 1e-3), (int) lrintf(fillratio*100.0f));
            //recvtext += String::formatted("%s | %d kb/s", recvfinfo.name.toRawUTF8(), lrintf(recvrate * 8 * 1e-3));
            recvtext += recvfinfo.name + String::formatted(" | %d kb/s", lrintf(recvrate * 8 * 1e-3));

            int64_t dropped = processor.getRemotePeerPacketsDropped(i);
            if (dropped > 0) {
                recvtext += String::formatted(" | %d drop", dropped);
            }

            if (dropped > pvf->lastDropped) {
                
                pvf->lastDroppedChangedTimestampMs = nowstampms;
            }

            int64_t resent = processor.getRemotePeerPacketsResent(i);
            if (resent > 0) {
                recvtext += String::formatted(" | %d resent", resent);
            }

            if (nowstampms < pvf->lastDroppedChangedTimestampMs + 1500) {
                pvf->recvActualBitrateLabel->setColour(Label::textColourId, droppedTextColor);
            }
            else {
                pvf->recvActualBitrateLabel->setColour(Label::textColourId, regularTextColor);
            }

            pvf->lastDropped = dropped;
        }
        else if (recvallow) {
            recvtext += TRANS("Other side is muted");    
            pvf->recvActualBitrateLabel->setColour(Label::textColourId, mutedTextColor);
        }
        else {
            recvtext += TRANS("You muted them");    
            pvf->recvActualBitrateLabel->setColour(Label::textColourId, mutedTextColor);
        }


        
        pvf->sendActualBitrateLabel->setText(sendtext, dontSendNotification);
        pvf->recvActualBitrateLabel->setText(recvtext, dontSendNotification);

        bool estlat = true;
        bool isreal = false;
        float totallat = processor.getRemotePeerTotalLatencyMs(i, isreal, estlat);
        
        pvf->pingLabel->setText(String::formatted("%d ms", (int)processor.getRemotePeerPingMs(i) ), dontSendNotification);

        if (!isreal) {
            if (pvf->stopLatencyTestTimestampMs > 0) {
                pvf->latencyLabel->setText(TRANS("****"), dontSendNotification);
            } else {
                pvf->latencyLabel->setText(TRANS("PRESS"), dontSendNotification);
            }
        } else {
            pvf->latencyLabel->setText(String::formatted("%d ms", (int)lrintf(totallat)) + (estlat ? "*" : ""), dontSendNotification);
            //pvf->latencyLabel->setText(String::formatted("%d ms", (int)totallat), dontSendNotification);
        }

        
        //pvf->statusLabel->setText(statustext, dontSendNotification);
        
        pvf->sendMutedButton->setToggleState(!processor.getRemotePeerSendAllow(i) , dontSendNotification);
        pvf->recvMutedButton->setToggleState(!processor.getRemotePeerRecvAllow(i) , dontSendNotification);

        pvf->latActiveButton->setToggleState(latactive, dontSendNotification);

        
        int autobufmode = (int)processor.getRemotePeerAutoresizeBufferMode(i);
        float buftimeMs = processor.getRemotePeerBufferTime(i);
        
        pvf->autosizeButton->setSelectedItemIndex(autobufmode, dontSendNotification);
        pvf->bufferLabel->setText(String::formatted("%d ms", (int) lrintf(buftimeMs)) + (autobufmode == 0 ? "" : autobufmode == 1 ? " (Auto+)" : " (Auto)"), dontSendNotification);

        
        if (!pvf->levelSlider->isMouseOverOrDragging()) {
            pvf->levelSlider->setValue(processor.getRemotePeerLevelGain(i), dontSendNotification);
        }
        if (!pvf->bufferTimeSlider->isMouseButtonDown()) {
            pvf->bufferTimeSlider->setValue(buftimeMs, dontSendNotification);
        }

        
        int formatindex = processor.getRemotePeerAudioCodecFormat(i);
        //if (formatindex != pvf->formatChoiceButton->getSelectedItemIndex()) {
            pvf->formatChoiceButton->setSelectedItemIndex(formatindex >= 0 ? formatindex : processor.getDefaultAudioCodecFormat(), dontSendNotification);
            pvf->sendQualityLabel->setText(processor.getAudioCodeFormatName(formatindex), dontSendNotification);
        //}
        
        pvf->recvMeter->setMeterSource (processor.getRemotePeerRecvMeterSource(i));        
        //pvf->sendMeter->setMeterSource (processor.getRemotePeerSendMeterSource(i));

        pvf->panSlider1->setValue(processor.getRemotePeerChannelPan(i, 0), dontSendNotification);
        pvf->panSlider2->setValue(processor.getRemotePeerChannelPan(i, 1), dontSendNotification);
        
        
        if (processor.getTotalNumOutputChannels() == 1) {
            pvf->panSlider1->setVisible(false);
            pvf->panSlider2->setVisible(false);
            pvf->panButton->setVisible(false);
        }
        else if (processor.getRemotePeerChannelCount(i) == 1) {
            pvf->panSlider1->setVisible(true);
            pvf->panSlider1->setDoubleClickReturnValue(true, 0.0);
            pvf->panSlider2->setVisible(false);
            pvf->panButton->setVisible(true);
        } else {
            pvf->panSlider1->setDoubleClickReturnValue(true, -1.0);
            pvf->panSlider2->setDoubleClickReturnValue(true, 1.0);
            pvf->panSlider1->setVisible(true);
            pvf->panSlider2->setVisible(true);
            pvf->panButton->setVisible(true);
        }
        
        //pvf->packetsizeSlider->setValue(findHighestSetBit(processor.getRemotePeerSendPacketsize(i)), dontSendNotification);
        
        //pvf->removeButton->setEnabled(connected);
        pvf->levelSlider->setEnabled(recvactive);
        //pvf->bufferTimeSlider->setEnabled(recvactive);
        //pvf->autosizeButton->setEnabled(recvactive);
        pvf->panSlider1->setEnabled(recvactive);
        pvf->panSlider2->setEnabled(recvactive);
        //pvf->formatChoiceButton->setEnabled(sendactive);

        const float disalpha = 0.4;
        //pvf->formatChoiceButton->setAlpha(sendactive ? 1.0 : disalpha);
        pvf->nameLabel->setAlpha(connected ? 1.0 : 0.8);
        pvf->addrLabel->setAlpha(connected ? 1.0 : 0.8);
        //pvf->sendMutedButton->setAlpha(connected ? 1.0 : 0.8);
        //pvf->recvMutedButton->setAlpha(connected ? 1.0 : 0.8);
        //pvf->latActiveButton->setAlpha(connected ? 1.0 : 0.8);
        //pvf->levelLabel->setAlpha(recvactive ? 1.0 : disalpha);
        pvf->statusLabel->setAlpha(connected ? 1.0 : disalpha);
        pvf->levelSlider->setAlpha(recvactive ? 1.0 : disalpha);
        //pvf->bufferTimeLabel->setAlpha(recvactive ? 1.0 : disalpha);
        //pvf->bufferTimeSlider->setAlpha(recvactive ? 1.0 : disalpha);
        //pvf->panSlider1->setAlpha(recvactive ? 1.0 : disalpha);
        //pvf->panSlider2->setAlpha(recvactive ? 1.0 : disalpha);
        //pvf->autosizeButton->setAlpha(recvactive ? 1.0 : 0.6);
        
        if (pvf->stopLatencyTestTimestampMs > 0.0 && nowstampms > pvf->stopLatencyTestTimestampMs
            && !pvf->latActiveButton->isMouseButtonDown()) {

            // only stop if it has actually gotten a real latency
            if (isreal) {
                stopLatencyTest(i);
                
                showPopTip(String::formatted(TRANS("Measured actual round-trip latency of %d ms"), (int) lrintf(totallat)), 4000, pvf->latActiveButton.get(), 140);

                //if (popTip && popTip->isVisible()) {
                //    Desktop::getInstance().getAnimator().fadeOut(popTip.get(), 200);
                //    popTip.reset();
                //}
            }
        }
    }
    
    lastUpdateTimestampMs = nowstampms;
}

void PeersContainerView::startLatencyTest(int i)
{
    if (i >= mPeerViews.size()) return;
    
    PeerViewInfo * pvf = mPeerViews.getUnchecked(i);

    pvf->stopLatencyTestTimestampMs = Time::getMillisecondCounter(); // make it stop after the first one  //+ 1500;
    pvf->wasRecvActiveAtLatencyTest = processor.getRemotePeerRecvActive(i);
    pvf->wasSendActiveAtLatencyTest = processor.getRemotePeerSendActive(i);
    
    pvf->latencyLabel->setText(TRANS("****"), dontSendNotification);

    //processor.setRemotePeerSendActive(i, false);
    //processor.setRemotePeerRecvActive(i, false);
    
    processor.startRemotePeerLatencyTest(i);             
}

void PeersContainerView::stopLatencyTest(int i)
{
    if (i >= mPeerViews.size()) return;
    PeerViewInfo * pvf = mPeerViews.getUnchecked(i);

    processor.stopRemotePeerLatencyTest(i);
    
    /*
    if (pvf->wasRecvActiveAtLatencyTest) {
        processor.setRemotePeerRecvActive(i, true);
    }
    if (pvf->wasSendActiveAtLatencyTest) {
        processor.setRemotePeerSendActive(i, true);
    }
    */
     
    pvf->stopLatencyTestTimestampMs = 0;
    
    bool estlat = true;
    bool isreal = false;
    float totallat = processor.getRemotePeerTotalLatencyMs(i, isreal, estlat);
    
    if (!isreal) {
        pvf->latencyLabel->setText(TRANS("PRESS"), dontSendNotification);
    } else {
        pvf->latencyLabel->setText(String::formatted("%d ms", (int)lrintf(totallat)) + (estlat ? "*" : "") , dontSendNotification);
    }
}

void PeersContainerView::timerCallback(int timerId)
{
    if (timerId == FillRatioUpdateTimerId) {
        for (int i=0; i < mPeerViews.size(); ++i) {
            PeerViewInfo * pvf = mPeerViews.getUnchecked(i);

            float ratio, stdev;
            if (processor.getRemotePeerReceiveBufferFillRatio(i, ratio, stdev) > 0) {
                pvf->jitterBufferMeter->setFillRatio(ratio, stdev);
            }
        }
    }
}


void PeersContainerView::choiceButtonSelected(SonoChoiceButton *comp, int index, int ident)
{
    for (int i=0; i < mPeerViews.size(); ++i) {
        PeerViewInfo * pvf = mPeerViews.getUnchecked(i);
        if (pvf->formatChoiceButton.get() == comp) {
            processor.setRemotePeerAudioCodecFormat(i, index);
            break;
        }        
        else if (pvf->autosizeButton.get() == comp) {
            processor.setRemotePeerAutoresizeBufferMode(i, (SonobusAudioProcessor::AutoNetBufferMode) index);
            break;
        }        
    }
}

void PeersContainerView::buttonClicked (Button* buttonThatWasClicked)
{
    for (int i=0; i < mPeerViews.size(); ++i) {
        PeerViewInfo * pvf = mPeerViews.getUnchecked(i);
        bool connected = processor.getRemotePeerConnected(i);
        //bool sendallow = processor.getRemotePeerSendAllow(i);
        //bool recvallow = processor.getRemotePeerRecvAllow(i);
        //bool sendactive = processor.getRemotePeerSendActive(i);
        //bool recvactive = processor.getRemotePeerRecvActive(i);
        bool isGroupPeer = processor.getRemotePeerUserName(i).isNotEmpty();
        
        if (pvf->sendMutedButton.get() == buttonThatWasClicked) {
            if (!connected && !isGroupPeer) {
                String hostname;
                int port = 0;
                processor.getRemotePeerAddressInfo(i, hostname, port);
                processor.connectRemotePeer(hostname, port);
            } else {
                // turns on allow and starts sending
                if (!pvf->sendMutedButton->getToggleState()) {
                    processor.setRemotePeerSendActive(i, true);
                } else {
                    // turns off sending and allow
                    processor.setRemotePeerSendAllow(i, false);
                }
            }
            break;
        }
        else if (pvf->recvMutedButton.get() == buttonThatWasClicked) {
            if (!connected && !isGroupPeer) {
                String hostname;
                int port = 0;
                processor.getRemotePeerAddressInfo(i, hostname, port);
                processor.connectRemotePeer(hostname, port);
            } else {
                if (!pvf->recvMutedButton->getToggleState()) {
                    // allows receiving and invites
                    processor.setRemotePeerRecvActive(i, true);             
                } else {
                    // turns off receiving and allow
                    processor.setRemotePeerRecvAllow(i, false);             
                }
            }
            break;
        }
        else if (pvf->latActiveButton.get() == buttonThatWasClicked) {
            if (pvf->latActiveButton->getToggleState()) {
                startLatencyTest(i);
                //showPopTip(TRANS("Measuring actual round-trip latency"), 4000, pvf->latActiveButton.get(), 140);
            } else {
                stopLatencyTest(i);
                //if (popTip && popTip->isVisible()) {
                //    Desktop::getInstance().getAnimator().fadeOut(popTip.get(), 200);
                //    popTip.reset();
                //}
            }
            break;
        }

        //else if (pvf->autosizeButton.get() == buttonThatWasClicked) {
            //processor.setRemotePeerAutoresizeBufferMode(i, (SonobusAudioProcessor::AutoNetBufferMode) pvf->autosizeButton->getSelectedItemIndex());
        //}
        
        else if (pvf->menuButton.get() == buttonThatWasClicked) {

            if (!optionsCalloutBox) {
                showOptions(i, true, pvf->menuButton.get());
            } else {
                showOptions(i, false);
            }

            break;
        }
        else if (pvf->panButton.get() == buttonThatWasClicked) {
            if (!pannerCalloutBox) {
                showPanners(i, true);
            } else {
                showPanners(i, false);
            }
            break;
        }
        else if (pvf->optionsButton.get() == buttonThatWasClicked) {
            if (!optionsCalloutBox) {
                showOptions(i, true);
            } else {
                showOptions(i, false);
            }
            break;
        }
        else if (pvf->optionsResetDropButton.get() == buttonThatWasClicked) {
            processor.resetRemotePeerPacketStats(i);
        }
        else if (pvf->optionsRemoveButton.get() == buttonThatWasClicked) {
            processor.removeRemotePeer(i);
            showOptions(i, false);
        }
    }    
}

void PeersContainerView::showPanners(int index, bool flag)
{
    
    if (flag && pannerCalloutBox == nullptr) {
        
        Viewport * wrap = new Viewport();
        
        Component* dw = this->findParentComponentOfClass<DocumentWindow>();
        
        if (!dw) {
            dw = this->findParentComponentOfClass<AudioProcessorEditor>();
        }
        if (!dw) {
            dw = this->findParentComponentOfClass<Component>();
        }
        if (!dw) {
            dw = this;
        }
        
        const int defWidth = 140;
#if JUCE_IOS
        const int defHeight = 50;
#else
        const int defHeight = 42;
#endif
        
        wrap->setSize(jmin(defWidth, dw->getWidth() - 20), jmin(defHeight, dw->getHeight() - 24));
        
        //Rectangle<int> setbounds = Rectangle<int>(5, mTitleImage->getBottom() + 2, std::min(100, getLocalBounds().getWidth() - 10), 80);
        
        auto * pvf = mPeerViews.getUnchecked(index);
        
        pvf->pannersContainer->setBounds(Rectangle<int>(0,0,defWidth,defHeight));
        
        //wrap->addAndMakeVisible(mSettingsPanel.get());
        wrap->setViewedComponent(pvf->pannersContainer.get(), false);
        pvf->pannersContainer->setVisible(true);
        
        pvf->pannerbox.performLayout(pvf->pannersContainer->getLocalBounds());
        
        
        Rectangle<int> bounds =  dw->getLocalArea(nullptr, pvf->panButton->getScreenBounds());
        DebugLogC("callout bounds: %s", bounds.toString().toRawUTF8());
        pannerCalloutBox = & CallOutBox::launchAsynchronously (wrap, bounds , dw);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(pannerCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }
    else {
        // dismiss it
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(pannerCalloutBox.get())) {
            box->dismiss();
            pannerCalloutBox = nullptr;
        }
    }
}

void PeersContainerView::showOptions(int index, bool flag, Component * fromView)
{
    
    if (flag && optionsCalloutBox == nullptr) {
        
        Viewport * wrap = new Viewport();
        
        Component* dw = this->findParentComponentOfClass<DocumentWindow>();
        
        if (!dw) {
            dw = this->findParentComponentOfClass<AudioProcessorEditor>();
        }
        if (!dw) {
            dw = this->findParentComponentOfClass<Component>();
        }
        if (!dw) {
            dw = this;
        }
        
        const int defWidth = 260;
#if JUCE_IOS
        const int defHeight = 180;
#else
        const int defHeight = 156;
#endif
        
        
        wrap->setSize(jmin(defWidth, dw->getWidth() - 20), jmin(defHeight, dw->getHeight() - 24));
        
        //Rectangle<int> setbounds = Rectangle<int>(5, mTitleImage->getBottom() + 2, std::min(100, getLocalBounds().getWidth() - 10), 80);
        
        auto * pvf = mPeerViews.getUnchecked(index);
        
        pvf->optionsContainer->setBounds(Rectangle<int>(0,0,defWidth,defHeight));
        
        wrap->setViewedComponent(pvf->optionsContainer.get(), false);
        pvf->optionsContainer->setVisible(true);
        
        pvf->optionsBox.performLayout(pvf->optionsContainer->getLocalBounds());
        pvf->bufferTimeLabel->setBounds(pvf->bufferTimeSlider->getBounds().removeFromLeft(pvf->bufferTimeSlider->getWidth()*0.5));

        
        Rectangle<int> bounds =  dw->getLocalArea(nullptr, fromView ? fromView->getScreenBounds() : pvf->menuButton->getScreenBounds());
        DebugLogC("callout bounds: %s", bounds.toString().toRawUTF8());
        optionsCalloutBox = & CallOutBox::launchAsynchronously (wrap, bounds , dw);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(optionsCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }
    else {
        // dismiss it
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(optionsCalloutBox.get())) {
            box->dismiss();
            optionsCalloutBox = nullptr;
        }
    }
}


void PeersContainerView::mouseUp (const MouseEvent& event) 
{
    for (int i=0; i < mPeerViews.size(); ++i) {
        PeerViewInfo * pvf = mPeerViews.getUnchecked(i);

        if (event.eventComponent == pvf->latActiveButton.get()) {
            uint32 nowtimems = Time::getMillisecondCounter();
            bool isreal=false, est = false;
            float latval = processor.getRemotePeerTotalLatencyMs(i, isreal, est);

            // only stop if it has actually gotten a real latency

            if (nowtimems >= pvf->stopLatencyTestTimestampMs && isreal) {
                stopLatencyTest(i);
                pvf->latActiveButton->setToggleState(false, dontSendNotification);
                showPopTip(String::formatted(TRANS("Measured actual round-trip latency of %d ms"), (int) lrintf(latval)), 4000, pvf->latActiveButton.get(), 140);
                //if (popTip && popTip->isVisible()) {
                //    Desktop::getInstance().getAnimator().fadeOut(popTip.get(), 200);
                //    popTip.reset();
                //}
            }
            break;
        }
        //else if (event.eventComponent == pvf->recvActualBitrateLabel.get()) {
            // reset stats
        //    processor.resetRemotePeerPacketStats(i);
        //}
        else if (event.eventComponent == pvf->recvMeter.get()) {
            pvf->recvMeter->clearClipIndicator(-1);
        }

    }
}

void PeersContainerView::showPopupMenu(Component * source, int index)
{
    if (index >= mPeerViews.size()) return;
    
    PeerViewInfo * pvf = mPeerViews.getUnchecked(index);
    bool isGroupPeer = processor.getRemotePeerUserName(index).isNotEmpty();

    Array<GenericItemChooserItem> items;
    if (isGroupPeer) {
        if (processor.getRemotePeerRecvAllow(index) && processor.getRemotePeerSendAllow(index)) {
            items.add(GenericItemChooserItem(TRANS("Mute All")));
        } else {
            items.add(GenericItemChooserItem(TRANS("Unmute All")));
        }
    } else {
        if (processor.getRemotePeerConnected(index)) {
            items.add(GenericItemChooserItem(TRANS("Mute All")));
        } else {
            items.add(GenericItemChooserItem(TRANS("Unmute All")));        
        }        
    }
    items.add(GenericItemChooserItem(TRANS("Remove")));
    
    Component* dw = source->findParentComponentOfClass<DocumentWindow>();

    if (!dw) {
        dw = source->findParentComponentOfClass<AudioProcessorEditor>();        
    }
    if (!dw) {
        dw = source->findParentComponentOfClass<Component>();        
    }
    
    //chooser->setSize(jmin(chooser->getWidth(), dw->getWidth() - 16), jmin(chooser->getHeight(), dw->getHeight() - 20));

    
    Rectangle<int> bounds =  dw->getLocalArea(nullptr, source->getScreenBounds());
    
    GenericItemChooser::launchPopupChooser(items, bounds, dw, this, index+1);
}

void PeersContainerView::genericItemChooserSelected(GenericItemChooser *comp, int index)
{
    // popup menu
    int vindex = comp->getTag() - 1;
    if (vindex >= mPeerViews.size()) return;        
    //PeerViewInfo * pvf = mPeerViews.getUnchecked(vindex);
    
    bool isGroupPeer = processor.getRemotePeerUserName(vindex).isNotEmpty();
    
    if (index == 0) {

        if (isGroupPeer) {
            if (!processor.getRemotePeerRecvAllow(vindex) || !processor.getRemotePeerSendAllow(vindex)) {
                // allow everything
                processor.setRemotePeerSendActive(vindex, true); 
                processor.setRemotePeerRecvActive(vindex, true);                 
            }
            else {
                // disallow everything
                processor.setRemotePeerSendAllow(vindex, false); 
                processor.setRemotePeerRecvAllow(vindex, false); 
            }
        }
        else {
            if (processor.getRemotePeerConnected(vindex)) {
                // disconnect
                processor.disconnectRemotePeer(vindex);  
            } else {
                
                String hostname;
                int port = 0;
                processor.getRemotePeerAddressInfo(vindex, hostname, port);
                processor.connectRemotePeer(hostname, port);            
            }
        }
    }
    else if (index == 1) {
        processor.removeRemotePeer(vindex);
    }
    
    
    if (CallOutBox* const cb = comp->findParentComponentOfClass<CallOutBox>()) {
        cb->dismiss();
    }

}


void PeersContainerView::sliderValueChanged (Slider* slider)
{
   for (int i=0; i < mPeerViews.size(); ++i) {
       PeerViewInfo * pvf = mPeerViews.getUnchecked(i);
       if (pvf->levelSlider.get() == slider) {
           processor.setRemotePeerLevelGain(i, pvf->levelSlider->getValue());   
       }
       else if (pvf->bufferTimeSlider.get() == slider) {
           float buftime = pvf->bufferTimeSlider->getValue();
           processor.setRemotePeerBufferTime(i, buftime);
           pvf->bufferTimeSlider->setValue(buftime, dontSendNotification);
       }
       else if (pvf->panSlider1.get() == slider) {
           processor.setRemotePeerChannelPan(i, 0, pvf->panSlider1->getValue());   
       }
       else if (pvf->panSlider2.get() == slider) {
           processor.setRemotePeerChannelPan(i, 1, pvf->panSlider2->getValue());   
       }

   }

}
