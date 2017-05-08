#include "preferences/dialog/dlgprefwaveform.h"

#include "mixxx.h"
#include "preferences/waveformsettings.h"
#include "waveform/waveformwidgetfactory.h"
#include "waveform/renderers/waveformwidgetrenderer.h"

DlgPrefWaveform::DlgPrefWaveform(QWidget* pParent, MixxxMainWindow* pMixxx,
                                 UserSettingsPointer pConfig, Library* pLibrary)
        : DlgPreferencePage(pParent),
          m_pConfig(pConfig),
          m_pLibrary(pLibrary),
          m_pMixxx(pMixxx) {
    setupUi(this);

    // Waveform overview init
    waveformOverviewComboBox->addItem(tr("Filtered")); // "0"
    waveformOverviewComboBox->addItem(tr("HSV")); // "1"
    waveformOverviewComboBox->addItem(tr("RGB")); // "2"

    // Populate waveform options.
    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();
    QVector<WaveformWidgetAbstractHandle> handles = factory->getAvailableTypes();
    for (int i = 0; i < handles.size(); ++i) {
        waveformTypeComboBox->addItem(handles[i].getDisplayName(),
                                      handles[i].getType());
    }

    // Populate zoom options.
    for (int i = WaveformWidgetRenderer::s_waveformMinZoom;
         i <= WaveformWidgetRenderer::s_waveformMaxZoom; i++) {
        defaultZoomComboBox->addItem(QString::number(100/double(i), 'f', 1) + " %");
    }

    // The GUI is not fully setup so connecting signals before calling
    // slotUpdate can generate rebootMixxxView calls.
    // TODO(XXX): Improve this awkwardness.
    slotUpdate();
    connect(frameRateSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotSetFrameRate(int)));
    connect(endOfTrackWarningTimeSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(slotSetWaveformEndRender(int)));
    connect(frameRateSlider, SIGNAL(valueChanged(int)),
            frameRateSpinBox, SLOT(setValue(int)));
    connect(frameRateSpinBox, SIGNAL(valueChanged(int)),
            frameRateSlider, SLOT(setValue(int)));
    connect(endOfTrackWarningTimeSlider, SIGNAL(valueChanged(int)),
            endOfTrackWarningTimeSpinBox, SLOT(setValue(int)));
    connect(endOfTrackWarningTimeSpinBox, SIGNAL(valueChanged(int)),
            endOfTrackWarningTimeSlider, SLOT(setValue(int)));

    connect(waveformTypeComboBox, SIGNAL(activated(int)),
            this, SLOT(slotSetWaveformType(int)));
    connect(defaultZoomComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotSetDefaultZoom(int)));
    connect(synchronizeZoomCheckBox, SIGNAL(clicked(bool)),
            this, SLOT(slotSetZoomSynchronization(bool)));
    connect(allVisualGain, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetVisualGainAll(double)));
    connect(lowVisualGain, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetVisualGainLow(double)));
    connect(midVisualGain, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetVisualGainMid(double)));
    connect(highVisualGain, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetVisualGainHigh(double)));
    connect(normalizeOverviewCheckBox, SIGNAL(toggled(bool)),
            this, SLOT(slotSetNormalizeOverview(bool)));
    connect(beatGridLinesCheckBox, SIGNAL(toggled(bool)),
            this, SLOT(slotSetGridLines(bool)));
    connect(factory, SIGNAL(waveformMeasured(float,int)),
            this, SLOT(slotWaveformMeasured(float,int)));
    connect(waveformOverviewComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotSetWaveformOverviewType(int)));
    connect(clearCachedWaveforms, SIGNAL(clicked()),
            this, SLOT(slotClearCachedWaveforms()));
}

DlgPrefWaveform::~DlgPrefWaveform() {
}

void DlgPrefWaveform::slotUpdate() {
    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();

    if (factory->isOpenGLAvailable()) {
        openGlStatusIcon->setText(factory->getOpenGLVersion());
    } else {
        openGlStatusIcon->setText(tr("OpenGL not available"));
    }

    WaveformWidgetType::Type currentType = factory->getType();
    int currentIndex = waveformTypeComboBox->findData(currentType);
    if (currentIndex != -1 && waveformTypeComboBox->currentIndex() != currentIndex) {
        waveformTypeComboBox->setCurrentIndex(currentIndex);
    }

    frameRateSpinBox->setValue(factory->getFrameRate());
    frameRateSlider->setValue(factory->getFrameRate());
    endOfTrackWarningTimeSpinBox->setValue(factory->getEndOfTrackWarningTime());
    endOfTrackWarningTimeSlider->setValue(factory->getEndOfTrackWarningTime());
    synchronizeZoomCheckBox->setChecked(factory->isZoomSync());
    allVisualGain->setValue(factory->getVisualGain(WaveformWidgetFactory::All));
    lowVisualGain->setValue(factory->getVisualGain(WaveformWidgetFactory::Low));
    midVisualGain->setValue(factory->getVisualGain(WaveformWidgetFactory::Mid));
    highVisualGain->setValue(factory->getVisualGain(WaveformWidgetFactory::High));
    normalizeOverviewCheckBox->setChecked(factory->isOverviewNormalized());
    defaultZoomComboBox->setCurrentIndex(factory->getDefaultZoom() - 1);
    beatGridLinesCheckBox->setChecked(factory->isBeatGridEnabled());

    // By default we set RGB woverview = "2"
    int overviewType = m_pConfig->getValue(
            ConfigKey("[Waveform]","WaveformOverviewType"), 2);
    if (overviewType != waveformOverviewComboBox->currentIndex()) {
        waveformOverviewComboBox->setCurrentIndex(overviewType);
    }

    WaveformSettings waveformSettings(m_pConfig);
    enableWaveformCaching->setChecked(waveformSettings.waveformCachingEnabled());
    enableWaveformGenerationWithAnalysis->setChecked(
        waveformSettings.waveformGenerationWithAnalysisEnabled());
    calculateCachedWaveformDiskUsage();
}

void DlgPrefWaveform::slotApply() {
    WaveformSettings waveformSettings(m_pConfig);
    waveformSettings.setWaveformCachingEnabled(enableWaveformCaching->isChecked());
    waveformSettings.setWaveformGenerationWithAnalysisEnabled(
        enableWaveformGenerationWithAnalysis->isChecked());
}

void DlgPrefWaveform::slotResetToDefaults() {
    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();

    // Get the default we ought to use based on whether the user has OpenGL or
    // not.
    WaveformWidgetType::Type defaultType = factory->autoChooseWidgetType();
    int defaultIndex = waveformTypeComboBox->findData(defaultType);
    if (defaultIndex != -1 && waveformTypeComboBox->currentIndex() != defaultIndex) {
        waveformTypeComboBox->setCurrentIndex(defaultIndex);
    }

    allVisualGain->setValue(1.0);
    lowVisualGain->setValue(1.0);
    midVisualGain->setValue(1.0);
    highVisualGain->setValue(1.0);

    // Default zoom level is 3 in WaveformWidgetFactory.
    defaultZoomComboBox->setCurrentIndex(3 + 1);

    // Don't synchronize zoom by default.
    synchronizeZoomCheckBox->setChecked(false);

    // RGB overview.
    waveformOverviewComboBox->setCurrentIndex(2);

    // Don't normalize overview.
    normalizeOverviewCheckBox->setChecked(false);

    // 30FPS is the default
    frameRateSlider->setValue(30);
    endOfTrackWarningTimeSlider->setValue(30);

    // Waveform caching enabled.
    enableWaveformCaching->setChecked(true);
    enableWaveformGenerationWithAnalysis->setChecked(false);

    // Grid lines on waveform is default
    beatGridLinesCheckBox->setChecked(true);
}

void DlgPrefWaveform::slotSetFrameRate(int frameRate) {
    WaveformWidgetFactory::instance()->setFrameRate(frameRate);
}

void DlgPrefWaveform::slotSetWaveformEndRender(int endTime) {
    WaveformWidgetFactory::instance()->setEndOfTrackWarningTime(endTime);
}

void DlgPrefWaveform::slotSetWaveformType(int index) {
    // Ignore sets for -1 since this happens when we clear the combobox.
    if (index < 0) {
        return;
    }
    WaveformWidgetFactory::instance()->setWidgetTypeFromHandle(index);
}

void DlgPrefWaveform::slotSetWaveformOverviewType(int index) {
    m_pConfig->set(ConfigKey("[Waveform]","WaveformOverviewType"), ConfigValue(index));
    m_pMixxx->rebootMixxxView();
}

void DlgPrefWaveform::slotSetDefaultZoom(int index) {
    WaveformWidgetFactory::instance()->setDefaultZoom(index + 1);
}

void DlgPrefWaveform::slotSetZoomSynchronization(bool checked) {
    WaveformWidgetFactory::instance()->setZoomSync(checked);
}

void DlgPrefWaveform::slotSetVisualGainAll(double gain) {
    WaveformWidgetFactory::instance()->setVisualGain(WaveformWidgetFactory::All,gain);
}

void DlgPrefWaveform::slotSetVisualGainLow(double gain) {
    WaveformWidgetFactory::instance()->setVisualGain(WaveformWidgetFactory::Low,gain);
}

void DlgPrefWaveform::slotSetVisualGainMid(double gain) {
    WaveformWidgetFactory::instance()->setVisualGain(WaveformWidgetFactory::Mid,gain);
}

void DlgPrefWaveform::slotSetVisualGainHigh(double gain) {
    WaveformWidgetFactory::instance()->setVisualGain(WaveformWidgetFactory::High,gain);
}

void DlgPrefWaveform::slotSetNormalizeOverview(bool normalize) {
    WaveformWidgetFactory::instance()->setOverviewNormalized(normalize);
}

void DlgPrefWaveform::slotWaveformMeasured(float frameRate, int droppedFrames) {
    frameRateAverage->setText(
            QString::number((double)frameRate, 'f', 2) + " : " +
            tr("dropped frames") + " " + QString::number(droppedFrames));
}

void DlgPrefWaveform::slotClearCachedWaveforms() {
    TrackCollection* pTrackCollection = m_pLibrary->getTrackCollection();
    if (pTrackCollection != nullptr) {
        AnalysisDao& analysisDao = pTrackCollection->getAnalysisDAO();
        analysisDao.deleteAnalysesByType(AnalysisDao::TYPE_WAVEFORM);
        analysisDao.deleteAnalysesByType(AnalysisDao::TYPE_WAVESUMMARY);
        calculateCachedWaveformDiskUsage();
    }
}

void DlgPrefWaveform::slotSetGridLines(bool displayGrid){
    WaveformWidgetFactory::instance()->setDisplayBeatGrid(displayGrid);
}

void DlgPrefWaveform::calculateCachedWaveformDiskUsage() {
    TrackCollection* pTrackCollection = m_pLibrary->getTrackCollection();
    if (pTrackCollection != nullptr) {
        AnalysisDao& analysisDao = pTrackCollection->getAnalysisDAO();
        size_t waveformBytes = analysisDao.getDiskUsageInBytes(AnalysisDao::TYPE_WAVEFORM);
        size_t wavesummaryBytes = analysisDao.getDiskUsageInBytes(AnalysisDao::TYPE_WAVESUMMARY);

        // Display total cached waveform size in mebibytes with 2 decimals.
        QString sizeMebibytes = QString::number(
                (waveformBytes + wavesummaryBytes) / (1024.0 * 1024.0), 'f', 2);

        waveformDiskUsage->setText(
                tr("Cached waveforms occupy %1 MiB on disk.").arg(sizeMebibytes));
    }
}
