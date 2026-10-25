#include "library/MusicLibrary.hpp"

#include <algorithm>
#include <set>

#include <attachedpictureframe.h>
#include <audioproperties.h>
#include <fileref.h>
#include <flacfile.h>
#include <flacpicture.h>
#include <id3v2tag.h>
#include <mpegfile.h>
#include <tag.h>
#include <tstring.h>
#include <vorbisfile.h>
#include <wavfile.h>
#include <xiphcomment.h>

// Временная заглушка вместо src/library/MusicLibrary.cpp: рекурсивный скан
// папки по расширениям, теги и обложки через TagLib — реальная реализация
// от Маскалика позже заменит этот файл.

namespace {

bool isAudioFile(const std::filesystem::path& path) {
    static const std::set<std::string> kExtensions = {".mp3", ".wav", ".ogg", ".flac"};
    return kExtensions.count(path.extension().string()) > 0;
}

QString toQString(const TagLib::String& string) {
    return QString::fromUtf8(string.toCString(true));
}

QByteArray toQByteArray(const TagLib::ByteVector& bytes) {
    return QByteArray(bytes.data(), static_cast<int>(bytes.size()));
}

// APIC-кадры ID3v2 (mp3, wav): предпочитаем FrontCover, иначе первый кадр.
QByteArray coverFromId3(TagLib::ID3v2::Tag* tag) {
    if (tag == nullptr) return {};
    QByteArray fallback;
    const TagLib::ID3v2::FrameList& frames = tag->frameList("APIC");
    for (const TagLib::ID3v2::Frame* raw : frames) {
        const auto* picture =
            dynamic_cast<const TagLib::ID3v2::AttachedPictureFrame*>(raw);
        if (picture == nullptr || picture->picture().size() == 0) continue;
        if (picture->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
            return toQByteArray(picture->picture());
        }
        if (fallback.isEmpty()) fallback = toQByteArray(picture->picture());
    }
    return fallback;
}

// Картинки FLAC/OGG (METADATA_BLOCK_PICTURE): тот же принцип отбора.
QByteArray coverFromPictures(const TagLib::List<TagLib::FLAC::Picture*>& pictures) {
    QByteArray fallback;
    for (const TagLib::FLAC::Picture* picture : pictures) {
        if (picture == nullptr || picture->data().size() == 0) continue;
        if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
            return toQByteArray(picture->data());
        }
        if (fallback.isEmpty()) fallback = toQByteArray(picture->data());
    }
    return fallback;
}

QByteArray readCover(const std::filesystem::path& path) {
    const std::string extension = path.extension().string();
    if (extension == ".mp3") {
        TagLib::MPEG::File file(path.c_str());
        return file.isValid() ? coverFromId3(file.ID3v2Tag()) : QByteArray();
    }
    if (extension == ".flac") {
        TagLib::FLAC::File file(path.c_str());
        return file.isValid() ? coverFromPictures(file.pictureList()) : QByteArray();
    }
    if (extension == ".wav") {
        TagLib::RIFF::WAV::File file(path.c_str());
        return file.isValid() ? coverFromId3(file.ID3v2Tag()) : QByteArray();
    }
    if (extension == ".ogg") {
        TagLib::Vorbis::File file(path.c_str());
        if (!file.isValid() || file.tag() == nullptr) return {};
        return coverFromPictures(file.tag()->pictureList());
    }
    return {};
}

qint64 fallbackDurationMs(const std::filesystem::path& path) {
    const auto hash = std::hash<std::string>{}(path.string());
    return 120000 + static_cast<qint64>(hash % 180001);
}

}  // namespace

MusicLibrary::MusicLibrary(QObject* parent) : QObject(parent) {}

void MusicLibrary::scan(const std::filesystem::path& dir) {
    tracks_.clear();

    std::error_code ec;
    if (std::filesystem::is_directory(dir, ec)) {
        std::filesystem::recursive_directory_iterator it(dir, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code fileEc;
            if (!it->is_regular_file(fileEc) || !isAudioFile(it->path())) continue;

            Track track;
            track.path = it->path();

            const TagLib::FileRef media(it->path().c_str());
            if (!media.isNull()) {
                if (const TagLib::Tag* tag = media.tag()) {
                    track.title = toQString(tag->title());
                    track.artist = toQString(tag->artist());
                    track.album = toQString(tag->album());
                }
                if (const TagLib::AudioProperties* properties = media.audioProperties()) {
                    track.durationMs = properties->lengthInMilliseconds();
                }
            }
            if (track.title.isEmpty()) {
                track.title = QString::fromStdString(it->path().stem().string());
            }
            if (track.durationMs <= 0) track.durationMs = fallbackDurationMs(it->path());
            track.coverData = readCover(it->path());

            tracks_.push_back(std::move(track));
        }
        std::sort(tracks_.begin(), tracks_.end(),
                  [](const Track& a, const Track& b) { return a.path < b.path; });
    }

    emit libraryChanged();
}

const std::vector<Track>& MusicLibrary::tracks() const {
    return tracks_;
}

void MusicLibrary::clear() {
    tracks_.clear();
    emit libraryChanged();
}
