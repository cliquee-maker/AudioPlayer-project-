#include "gui/SimulatedVisualSource.hpp"

#include "audio/AudioEngine.hpp"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

constexpr int kBins = 48;
constexpr int kWaveformSamples = 256;

// Частота анализа: достаточно для спектра до ~10 кГц и не раздувает память
// на длинных треках (моно float, ~88 КБ/с).
constexpr int kAnalysisRate = 22050;
constexpr int kFftSize = 2048;
constexpr double kMinFreq = 40.0;
constexpr double kMaxFreq = 10000.0;
constexpr double kPi = 3.14159265358979323846;

void fft(std::vector<float>& re, std::vector<float>& im) {
    const int n = static_cast<int>(re.size());

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[static_cast<std::size_t>(i)], re[static_cast<std::size_t>(j)]);
            std::swap(im[static_cast<std::size_t>(i)], im[static_cast<std::size_t>(j)]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * kPi / len;
        const float wLenRe = static_cast<float>(std::cos(ang));
        const float wLenIm = static_cast<float>(std::sin(ang));
        const int half = len / 2;
        for (int i = 0; i < n; i += len) {
            float wRe = 1.0f;
            float wIm = 0.0f;
            for (int j = 0; j < half; ++j) {
                const auto a = static_cast<std::size_t>(i + j);
                const auto b = static_cast<std::size_t>(i + j + half);
                const float uRe = re[a];
                const float uIm = im[a];
                const float vRe = re[b] * wRe - im[b] * wIm;
                const float vIm = re[b] * wIm + im[b] * wRe;
                re[a] = uRe + vRe;
                im[a] = uIm + vIm;
                re[b] = uRe - vRe;
                im[b] = uIm - vIm;
                const float nextRe = wRe * wLenRe - wIm * wLenIm;
                wIm = wRe * wLenIm + wIm * wLenRe;
                wRe = nextRe;
            }
        }
    }
}

float readChannelSample(const uchar* data, qint64 sampleIndex, QAudioFormat::SampleFormat format) {
    switch (format) {
    case QAudioFormat::UInt8:
        return (static_cast<float>(data[sampleIndex]) - 128.0f) / 128.0f;
    case QAudioFormat::Int16:
        return static_cast<float>(reinterpret_cast<const qint16*>(data)[sampleIndex]) / 32768.0f;
    case QAudioFormat::Int32:
        return static_cast<float>(reinterpret_cast<const qint32*>(data)[sampleIndex]) /
               2147483648.0f;
    case QAudioFormat::Float:
        return reinterpret_cast<const float*>(data)[sampleIndex];
    case QAudioFormat::Unknown:
    case QAudioFormat::NSampleFormats:
        break;
    }
    return 0.0f;
}

}  // namespace

SimulatedVisualSource::SimulatedVisualSource(AudioEngine& engine)
    : engine_(engine), bins_(kBins, 0.0f), waveform_(kWaveformSamples, 0.0f) {
    clock_.start();
}

SimulatedVisualSource::~SimulatedVisualSource() {
    stopDecoder();
}

void SimulatedVisualSource::setTrack(const std::filesystem::path& file) {
    if (file == trackPath_ && !decodingFailed_) return;
    trackPath_ = file;

    stopDecoder();
    pcm_.clear();
    inputFrames_ = 0;
    nextOutputFrame_ = 0;
    decodingFailed_ = false;
    smoothPositionMs_ = -1.0;
    lastWaveformTickMs_ = -1;
    lastDisplayPositionMs_ = -1;
    displayWave_.clear();
    std::fill(bins_.begin(), bins_.end(), 0.0f);
    std::fill(waveform_.begin(), waveform_.end(), 0.0f);

    if (file.empty()) return;
    startDecoder();
}

void SimulatedVisualSource::startDecoder() {
    decoder_ = new QAudioDecoder();
    decoder_->setSource(QUrl::fromLocalFile(QString::fromStdString(trackPath_.u8string())));

    QObject::connect(decoder_, &QAudioDecoder::bufferReady, decoder_, [this] {
        while (decoder_->bufferAvailable()) appendBuffer(decoder_->read());
    });
    QObject::connect(decoder_, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), decoder_,
                     [this](QAudioDecoder::Error error) {
                         if (error != QAudioDecoder::NoError) decodingFailed_ = true;
                     });

    decoder_->start();
}

void SimulatedVisualSource::stopDecoder() {
    if (decoder_ == nullptr) return;
    decoder_->stop();
    delete decoder_;
    decoder_ = nullptr;
}

void SimulatedVisualSource::appendBuffer(const QAudioBuffer& buffer) {
    const QAudioFormat format = buffer.format();
    const int channels = format.channelCount();
    const int frames = buffer.frameCount();
    const int rate = format.sampleRate();
    if (channels <= 0 || frames <= 0 || rate <= 0) return;

    const uchar* data = buffer.constData<uchar>();
    const auto sampleFormat = format.sampleFormat();
    const auto monoAt = [&](qint64 frame) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            sum += readChannelSample(data, frame * channels + c, sampleFormat);
        }
        return sum / static_cast<float>(channels);
    };

    const double ratio = static_cast<double>(rate) / kAnalysisRate;
    const qint64 bufferStart = inputFrames_;
    const qint64 bufferEnd = bufferStart + frames;

    // Пересэмплирование с непрерывным индексом: буферы декодера не обязаны
    // совпадать с границами наших отсчётов.
    qint64 out = nextOutputFrame_;
    while (static_cast<double>(out) * ratio < static_cast<double>(bufferEnd)) {
        const double src = static_cast<double>(out) * ratio;
        const auto srcFloor = static_cast<qint64>(std::floor(src));
        const float frac = static_cast<float>(src - static_cast<double>(srcFloor));

        const qint64 local = std::clamp<qint64>(srcFloor - bufferStart, 0, frames - 1);
        const qint64 next = std::min<qint64>(local + 1, frames - 1);
        const float a = monoAt(local);
        const float b = monoAt(next);
        pcm_.push_back(a + (b - a) * frac);
        ++out;
    }

    nextOutputFrame_ = out;
    inputFrames_ = bufferEnd;
}

const std::vector<float>& SimulatedVisualSource::bins() const {
    const qint64 now = clock_.elapsed();
    qint64 delta = now - lastMs_;
    lastMs_ = now;
    if (delta <= 0 || delta > 250) delta = 16;

    if (hasRealAudio()) {
        updateBinsFromPcm(static_cast<float>(delta));
    } else {
        updateBinsSynthetic(static_cast<float>(delta));
    }
    return bins_;
}

void SimulatedVisualSource::updateBinsFromPcm(float deltaMs) const {
    const qint64 posSamples =
        engine_.positionMs() * static_cast<qint64>(kAnalysisRate) / 1000;
    const qint64 windowStart = posSamples - kFftSize;

    std::vector<float> re(static_cast<std::size_t>(kFftSize), 0.0f);
    std::vector<float> im(static_cast<std::size_t>(kFftSize), 0.0f);
    for (int i = 0; i < kFftSize; ++i) {
        const qint64 idx = windowStart + i;
        if (idx < 0 || idx >= static_cast<qint64>(pcm_.size())) continue;
        const double hann = 0.5 - 0.5 * std::cos(2.0 * kPi * i / (kFftSize - 1));
        re[static_cast<std::size_t>(i)] =
            pcm_[static_cast<std::size_t>(idx)] * static_cast<float>(hann);
    }
    fft(re, im);

    const float scale =
        engine_.isPlaying() ? std::clamp(engine_.volume() / 100.0f, 0.0f, 1.0f) : 0.0f;
    const double binWidth = static_cast<double>(kAnalysisRate) / kFftSize;
    const double ratio = kMaxFreq / kMinFreq;

    const float attack = 1.0f - std::exp(-deltaMs / 40.0f);
    const float release = 1.0f - std::exp(-deltaMs / 160.0f);

    for (int i = 0; i < kBins; ++i) {
        const double f0 = kMinFreq * std::pow(ratio, static_cast<double>(i) / kBins);
        const double f1 = kMinFreq * std::pow(ratio, static_cast<double>(i + 1) / kBins);
        const int k0 = std::max(1, static_cast<int>(std::floor(f0 / binWidth)));
        const int k1 = std::max(k0, static_cast<int>(std::ceil(f1 / binWidth)) - 1);

        float peak = 0.0f;
        for (int k = k0; k <= k1 && k < kFftSize / 2; ++k) {
            const auto idx = static_cast<std::size_t>(k);
            const float mag = std::sqrt(re[idx] * re[idx] + im[idx] * im[idx]);
            peak = std::max(peak, mag);
        }

        // Полный синус даёт |X| = A * N/4 после окна Ханна.
        const float amplitude = peak * 4.0f / kFftSize;
        const float db = 20.0f * std::log10(std::max(amplitude, 1e-6f));
        const float normalized = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
        const float target = std::pow(normalized, 0.8f) * scale;

        float& value = bins_[static_cast<std::size_t>(i)];
        const float k = target > value ? attack : release;
        value += (target - value) * k;
    }
}

void SimulatedVisualSource::updateBinsSynthetic(float deltaMs) const {
    const double t = static_cast<double>(engine_.positionMs()) / 1000.0;
    const float targetScale = engine_.isPlaying() ? engine_.volume() / 100.0f : 0.0f;

    for (int i = 0; i < kBins; ++i) {
        const double phase = i * 0.37;
        double v = 0.5 + 0.5 * std::sin(t * (1.7 + i * 0.06) + phase);
        v *= 0.55 + 0.45 * std::sin(t * 0.9 + i * 0.23 + 1.3);
        v = std::pow(std::max(v, 0.0), 1.4);
        const double tilt = 1.0 - 0.55 * (static_cast<double>(i) / kBins);
        const float target = static_cast<float>(v * tilt) * targetScale;

        float& value = bins_[static_cast<std::size_t>(i)];
        const float tau = target > value ? 60.0f : 190.0f;
        const float k = 1.0f - std::exp(-deltaMs / tau);
        value += (target - value) * k;
    }
}

const std::vector<float>& SimulatedVisualSource::waveform() const {
    const qint64 positionMs = advanceVisualPosition();
    if (hasRealAudio()) {
        updateWaveformFromPcm(positionMs);
    } else {
        updateWaveformSynthetic(positionMs);
    }
    return waveform_;
}

qint64 SimulatedVisualSource::advanceVisualPosition() const {
    const qint64 now = clock_.elapsed();
    const qint64 engineMs = engine_.positionMs();

    if (!engine_.isPlaying()) {
        smoothPositionMs_ = static_cast<double>(engineMs);
        lastWaveformTickMs_ = now;
        return engineMs;
    }

    if (smoothPositionMs_ < 0.0 || lastWaveformTickMs_ < 0) {
        smoothPositionMs_ = static_cast<double>(engineMs);
        lastWaveformTickMs_ = now;
        return engineMs;
    }

    qint64 delta = now - lastWaveformTickMs_;
    lastWaveformTickMs_ = now;
    delta = std::clamp<qint64>(delta, 0, 250);

    // Перемотка/смена трека: позиция плеера ушла далеко — подстраиваемся.
    if (std::abs(smoothPositionMs_ - static_cast<double>(engineMs)) > 300.0) {
        smoothPositionMs_ = static_cast<double>(engineMs);
    } else {
        smoothPositionMs_ += static_cast<double>(delta);
    }
    return static_cast<qint64>(smoothPositionMs_);
}

void SimulatedVisualSource::updateWaveformFromPcm(qint64 positionMs) const {
    // Берём окно шире экранного (100 мс вместо 50): запас нужен, чтобы
    // сдвинуть картинку к предыдущему кадру и не дать ей «бежать».
    constexpr int kRawSamples = kWaveformSamples * 2;
    constexpr double kRawWindowSec = 0.10;
    const qint64 posSamples = positionMs * static_cast<qint64>(kAnalysisRate) / 1000;
    const auto rawSpan = static_cast<qint64>(kAnalysisRate * kRawWindowSec);
    const qint64 start = posSamples - rawSpan;
    const qint64 pcmSize = static_cast<qint64>(pcm_.size());

    const float scale =
        engine_.isPlaying() ? std::clamp(engine_.volume() / 100.0f, 0.0f, 1.0f) : 0.0f;

    std::vector<float> raw(kRawSamples, 0.0f);
    for (int j = 0; j < kRawSamples; ++j) {
        const qint64 a = start + rawSpan * j / kRawSamples;
        const qint64 b = std::max(a + 1, start + rawSpan * (j + 1) / kRawSamples);

        double sum = 0.0;
        int count = 0;
        for (qint64 s = a; s < b; ++s) {
            if (s < 0 || s >= pcmSize) continue;
            sum += pcm_[static_cast<std::size_t>(s)];
            ++count;
        }
        const float value = count > 0 ? static_cast<float>(sum / count) : 0.0f;
        raw[static_cast<std::size_t>(j)] = std::clamp(value * scale, -1.0f, 1.0f);
    }

    constexpr int kHalf = kRawSamples / 2;
    const bool reset = displayWave_.size() != static_cast<std::size_t>(kWaveformSamples) ||
                       lastDisplayPositionMs_ < 0 ||
                       std::abs(positionMs - lastDisplayPositionMs_) > 250;
    if (reset) {
        // Перемотка/новый трек: показываем текущее окно как есть.
        displayWave_.assign(raw.begin() + kHalf, raw.end());
    } else {
        // Ищем в широком окне участок, который лучше всего совпадает
        // с предыдущим кадром. Это удерживает фазу на месте: сигнал
        // меняется по амплитуде, но не уезжает вбок.
        constexpr int kMaxShift = 3 * kHalf / 4;
        int bestShift = 0;
        double bestScore = std::numeric_limits<double>::max();
        for (int shift = -kMaxShift; shift <= 0; ++shift) {
            const int base = kHalf + shift;
            double error = 0.0;
            double targetEnergy = 0.0;
            double candidateEnergy = 0.0;
            for (int i = 0; i < kWaveformSamples; ++i) {
                const double candidate = raw[static_cast<std::size_t>(base + i)];
                const double target = displayWave_[static_cast<std::size_t>(i)];
                const double diff = candidate - target;
                error += diff * diff;
                targetEnergy += target * target;
                candidateEnergy += candidate * candidate;
            }
            const double score =
                error / (std::sqrt(targetEnergy * candidateEnergy) + 1e-9);
            if (score < bestScore) {
                bestScore = score;
                bestShift = shift;
            }
        }

        const int base = kHalf + bestShift;
        constexpr float kBlend = 0.45f;
        for (int i = 0; i < kWaveformSamples; ++i) {
            const float aligned = raw[static_cast<std::size_t>(base + i)];
            float& value = displayWave_[static_cast<std::size_t>(i)];
            value += (aligned - value) * kBlend;
        }
    }

    waveform_ = displayWave_;
    lastDisplayPositionMs_ = positionMs;
}

void SimulatedVisualSource::updateWaveformSynthetic(qint64 positionMs) const {
    const double t = static_cast<double>(positionMs) / 1000.0;
    const float scale = engine_.isPlaying() ? engine_.volume() / 100.0f : 0.0f;

    // Форма стоит на месте (фаза зависит только от x), а со временем
    // медленно меняются амплитуда и наклон — без горизонтального бега.
    for (int j = 0; j < kWaveformSamples; ++j) {
        const double u = static_cast<double>(j) / (kWaveformSamples - 1);
        const double s =
            0.5 * std::sin(2.0 * kPi * 5.0 * u + 1.7 * std::sin(2.0 * kPi * 0.8 * t)) +
            0.3 * std::sin(2.0 * kPi * 10.0 * u + 0.5 * std::sin(2.0 * kPi * 2.3 * t)) +
            0.2 * std::sin(2.0 * kPi * 16.0 * u + std::sin(2.0 * kPi * 5.1 * t));
        waveform_[static_cast<std::size_t>(j)] =
            std::clamp(static_cast<float>(s) * scale, -1.0f, 1.0f);
    }
}

bool SimulatedVisualSource::hasRealAudio() const {
    if (pcm_.empty()) return false;
    const qint64 posSamples =
        engine_.positionMs() * static_cast<qint64>(kAnalysisRate) / 1000;
    return posSamples >= kFftSize && posSamples < static_cast<qint64>(pcm_.size());
}

bool SimulatedVisualSource::isPlaying() const {
    return engine_.isPlaying();
}

float SimulatedVisualSource::volume() const {
    return engine_.volume();
}

qint64 SimulatedVisualSource::positionMs() const {
    return engine_.positionMs();
}

qint64 SimulatedVisualSource::durationMs() const {
    return engine_.durationMs();
}
