#include <iostream>
#include <string>
#include <algorithm>
#include <cinttypes>

#include "../cgssh.h"
#include "../../lib/cgss_api.h"
#include "../common/common.h"

using namespace cgss;
using namespace std;

struct Acb2WavsOptions {
    HCA_DECODER_CONFIG decoderConfig;
    bool_t useCueName;
};

static void PrintHelp();

static int ParseArgs(int argc, const char *argv[], const char **input, Acb2WavsOptions &options);

static int DoWork(const string &inputFile, const Acb2WavsOptions &options);

static int
ProcessAllBinaries(CAcbFile *acb, uint32_t formatVersion, const Acb2WavsOptions &options, const string &extractDir, CAfs2Archive *archive, IStream *dataStream, bool_t isInternal);

static int DecodeHca(IStream *hcaDataStream, IStream *waveStream, const HCA_DECODER_CONFIG &dc);

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    const char *inputFile;
    Acb2WavsOptions options = {0};

    const auto parsed = ParseArgs(argc, argv, &inputFile, options);

    if (parsed < 0) {
        return 0;
    } else if (parsed > 0) {
        return parsed;
    }

    return DoWork(inputFile, options);
}

static void PrintHelp() {
    cout << "Usage:\n" << endl;
    cout << "acb2wavs <acb file> [-a <key1> -b <key2>] [-n]" << endl << endl;
    cout << "\t-n\tUse cue names for output waveforms" << endl;
}

static int ParseArgs(int argc, const char *argv[], const char **input, Acb2WavsOptions &options) {
    if (argc < 2) {
        PrintHelp();
        return -1;
    }

    *input = argv[1];

    options.decoderConfig.waveHeaderEnabled = TRUE;
    options.decoderConfig.decodeFunc = CDefaultWaveGenerator::Decode16BitS;

    options.decoderConfig.cipherConfig.keyModifier = 0;

    for (int i = 2; i < argc; ++i) {
        if (argv[i][0] == '-' || argv[i][0] == '/') {
            switch (argv[i][1]) {
                case 'a':
                    if (i + 1 < argc) {
                        options.decoderConfig.cipherConfig.keyParts.key1 = atoh<uint32_t>(argv[++i]);
                    }
                    break;
                case 'b':
                    if (i + 1 < argc) {
                        options.decoderConfig.cipherConfig.keyParts.key2 = atoh<uint32_t>(argv[++i]);
                    }
                    break;
                case 'n':
                    options.useCueName = TRUE;
                    break;
                default:
                    return 2;
            }
        }
    }

    return 0;
}

static int DoWork(const string &inputFile, const Acb2WavsOptions &options) {
    const auto baseExtractDirPath = CPath::Combine(CPath::GetDirectoryName(inputFile), "_acb_" + CPath::GetFileName(inputFile));

    CFileStream fileStream(inputFile.c_str(), FileMode::OpenExisting, FileAccess::Read);
    CAcbFile acb(&fileStream, inputFile.c_str());

    acb.Initialize();

    CAfs2Archive *archive = nullptr;
    uint32_t formatVersion = acb.GetFormatVersion();
    int r;

    try {
        archive = acb.GetInternalAwb();
    } catch (CException &ex) {
        fprintf(stderr, "%s (%d)\n", ex.GetExceptionMessage().c_str(), ex.GetOpResult());
        archive = nullptr;
    }

    if (archive) {
        const auto extractDir = CPath::Combine(baseExtractDirPath, "internal");

        try {
            r = ProcessAllBinaries(&acb, formatVersion, options, extractDir, archive, acb.GetStream(), TRUE);
        } catch (CException &ex) {
            fprintf(stderr, "%s (%d)\n", ex.GetExceptionMessage().c_str(), ex.GetOpResult());
            r = -1;
        }

        delete archive;

        if (r != 0) {
            return r;
        }
    }

    try {
        archive = acb.GetExternalAwb();
    } catch (CException &ex) {
        fprintf(stderr, "%s (%d)\n", ex.GetExceptionMessage().c_str(), ex.GetOpResult());
        archive = nullptr;
    }

    if (archive) {
        const auto extractDir = CPath::Combine(baseExtractDirPath, "external");

        try {
            CFileStream fs(archive->GetFileName(), FileMode::OpenExisting, FileAccess::Read);

            r = ProcessAllBinaries(&acb, formatVersion, options, extractDir, archive, &fs, FALSE);
        } catch (CException &ex) {
            fprintf(stderr, "%s (%d)\n", ex.GetExceptionMessage().c_str(), ex.GetOpResult());
            r = -1;
        }

        delete archive;

        if (r != 0) {
            return r;
        }
    }

    return 0;
}

static int
ProcessAllBinaries(CAcbFile *acb, uint32_t formatVersion, const Acb2WavsOptions &options, const string &extractDir, CAfs2Archive *archive, IStream *dataStream, bool_t isInternal) {
    if (!CFileSystem::DirectoryExists(extractDir)) {
        if (!CFileSystem::MkDir(extractDir)) {
            fprintf(stderr, "Failed to create directory %s.\n", extractDir.c_str());
            return -1;
        }
    }

    const auto afsSource = isInternal ? "internal" : "external";
    HCA_DECODER_CONFIG decoderConfig = options.decoderConfig;

    if (formatVersion >= CAcbFile::KEY_MODIFIER_ENABLED_VERSION) {
        decoderConfig.cipherConfig.keyModifier = archive->GetHcaKeyModifier();
    } else {
        decoderConfig.cipherConfig.keyModifier = 0;
    }

    for (auto &entry : archive->GetFiles()) {
        auto &record = entry.second;

        auto fileData = CAcbHelper::ExtractToNewStream(dataStream, record.fileOffsetAligned, (uint32_t)record.fileSize);

        const auto isHca = CHcaFormatReader::IsPossibleHcaStream(fileData);

        fprintf(stdout, "Processing %s AFS: #%" PRIu32 " (offset=%" PRIu32 ", size=%" PRIu32 ")",
                afsSource, (uint32_t)record.cueId, (uint32_t)record.fileOffsetAligned, (uint32_t)record.fileSize);

        int r;

        if (isHca) {
            std::string extractFileName;

            if (options.useCueName) {
                extractFileName = acb->GetCueNameFromCueId(record.cueId);
            } else {
                extractFileName = CAcbFile::GetSymbolicFileNameFromCueId(record.cueId);
            }

            extractFileName = common_utils::ReplaceAnyExtension(extractFileName, ".wav");

            auto extractFilePath = CPath::Combine(extractDir, extractFileName);

            fprintf(stdout, " to %s...\n", extractFilePath.c_str());

            try {
                CFileStream fs(extractFilePath.c_str(), FileMode::Create, FileAccess::Write);

                r = DecodeHca(fileData, &fs, decoderConfig);

                if (r == 0) {
                    fprintf(stdout, "decoded\n");
                } else {
                    fprintf(stdout, "errored\n");
                }
            } catch (CException &ex) {
                if (CFileSystem::FileExists(extractFilePath)) {
                    CFileSystem::RmFile(extractFilePath);
                }

                fprintf(stdout, "errored: %s (%d)\n", ex.GetExceptionMessage().c_str(), ex.GetOpResult());
            }
        } else {
            fprintf(stdout, "... skipped (not HCA)\n");
        }

        delete fileData;
    }

    return 0;
}

static int DecodeHca(IStream *hcaDataStream, IStream *waveStream, const HCA_DECODER_CONFIG &dc) {
    CHcaDecoder decoder(hcaDataStream, dc);
    static const int bufferSize = 10240;
    uint8_t buffer[bufferSize];
    uint32_t read = 1;

    while (read > 0) {
        read = decoder.Read(buffer, bufferSize, 0, bufferSize);

        if (read > 0) {
            waveStream->Write(buffer, bufferSize, 0, read);
        }
    }

    return 0;
}
