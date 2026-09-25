module;
#include <QCoreApplication>
#include <QFileInfo>
#include <QObject>
#include <QStringList>
#include <QTextStream>

module ArtifactPr.CLI;

import ArtifactPr.EditorEngine;
import ArtifactPr.SequenceExporter;

namespace {
void printUsage() { QTextStream out(stdout); out << "ArtifactPr CLI\nUsage: ArtifactPr --cli <command> [arguments]\nCommands: project open|save|info; media list; sequence list|select <id>; clip list; undo; redo; export <output> [--start <frame>] [--end <frame>] [--no-audio]\n"; }
int fail(const QString& m) { QTextStream(stderr) << "ArtifactPr: " << m << '\n'; return 1; }
int requireProject() { auto* e=ArtifactPr::EditorEngine::instance(); return e->currentProject().id.isEmpty() ? fail(QStringLiteral("no project is open")) : 0; }
int listProjectInfo() { const auto p=ArtifactPr::EditorEngine::instance()->currentProject(); QTextStream o(stdout); o << "project: " << (p.id.isEmpty()?QStringLiteral("<none>"):p.id) << '\n' << "name: " << p.name << '\n' << "active-sequence: " << p.activeSequenceId << '\n' << "sequences: " << p.sequences.size() << '\n' << "media: " << p.mediaPool.size() << '\n'; return 0; }
int listMedia() { if(int r=requireProject()) return r; QTextStream o(stdout); for(const auto& m:ArtifactPr::EditorEngine::instance()->mediaPool()) o<<m.id<<'\t'<<m.type<<'\t'<<m.filePath<<'\n'; return 0; }
int listSequences() { if(int r=requireProject()) return r; QTextStream o(stdout); for(const auto& s:ArtifactPr::EditorEngine::instance()->currentProject().sequences) o<<s.id<<'\t'<<s.name<<'\t'<<s.resolution<<'\t'<<s.frameRate<<'\n'; return 0; }
int addMedia(const QStringList& a) { if(int r=requireProject()) return r; if(a.size()<4)return fail(QStringLiteral("media add requires <file> [video|audio|image]")); auto* e=ArtifactPr::EditorEngine::instance(); const QString type=a.size()>3?a[3]:QStringLiteral("video"); if(type != QStringLiteral("video") && type != QStringLiteral("audio") && type != QStringLiteral("image")) return fail(QStringLiteral("media type must be video, audio, or image")); e->addMediaToPool(a[2],QFileInfo(a[2]).fileName(),type); return 0; }
int selectClip(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<4)return fail(QStringLiteral("clip select requires <id>")); auto* e=ArtifactPr::EditorEngine::instance(); if(!e->findClip(a[3]))return fail(QStringLiteral("clip not found")); e->selectClip(a[3]); return 0; }
int moveClip(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<5)return fail(QStringLiteral("clip move requires <id> <start-frame>")); bool ok=false; const auto n=a[4].toLongLong(&ok); if(!ok)return fail(QStringLiteral("invalid start frame")); auto* e=ArtifactPr::EditorEngine::instance(); if(!e->findClip(a[3]))return fail(QStringLiteral("clip not found")); e->moveClip(a[3],n); return 0; }
int trimClip(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<7)return fail(QStringLiteral("clip trim requires <id> <start> <duration> <source-in> <source-out>")); bool ok=true; ArtifactPr::FramePosition n[4]; auto* e=ArtifactPr::EditorEngine::instance(); for(int i=0;i<4;++i){n[i]=a[4+i].toLongLong(&ok);if(!ok)return fail(QStringLiteral("invalid trim value"));} if(!e->findClip(a[3]))return fail(QStringLiteral("clip not found")); e->trimClip(a[3],n[0],n[1],n[2],n[3]); return 0; }

int createProject(const QStringList& a) { if (a.size() < 4) return fail(QStringLiteral("project create requires <file>")); auto* e=ArtifactPr::EditorEngine::instance(); e->newProject(); return e->saveProject(a[3]) ? 0 : 1; }
int newSequence() { if (int r=requireProject()) return r; ArtifactPr::EditorEngine::instance()->newSequence(); return 0; }
int deleteClip(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<4)return fail(QStringLiteral("clip delete requires <id>")); auto* e=ArtifactPr::EditorEngine::instance(); if(!e->findClip(a[3]))return fail(QStringLiteral("clip not found")); e->selectClip(a[3]); e->deleteSelectedClip(); return 0; }
int rippleDeleteClip(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<4)return fail(QStringLiteral("clip ripple-delete requires <id>")); auto* e=ArtifactPr::EditorEngine::instance(); if(!e->findClip(a[3]))return fail(QStringLiteral("clip not found")); e->rippleDeleteClipAt(a[3]); return 0; }
int splitClip(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<5)return fail(QStringLiteral("clip split requires <id> <frame>")); bool ok=false; const auto frame=a[4].toLongLong(&ok); if(!ok)return fail(QStringLiteral("invalid split frame")); auto* e=ArtifactPr::EditorEngine::instance(); if(!e->findClip(a[3]))return fail(QStringLiteral("clip not found")); e->setCurrentFrame(frame); e->selectClip(a[3]); e->splitClipAtPlayhead(); return 0; }
int seekFrame(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<4)return fail(QStringLiteral("seek requires <frame>")); bool ok=false; const auto frame=a[3].toLongLong(&ok); if(!ok)return fail(QStringLiteral("invalid frame")); ArtifactPr::EditorEngine::instance()->setCurrentFrame(frame); return 0; }
int setIn(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<4)return fail(QStringLiteral("in requires <frame>")); bool ok=false; const auto frame=a[3].toLongLong(&ok); if(!ok)return fail(QStringLiteral("invalid frame")); ArtifactPr::EditorEngine::instance()->setInPoint(frame); return 0; }
int setOut(const QStringList& a) { if(int r=requireProject())return r; if(a.size()<4)return fail(QStringLiteral("out requires <frame>")); bool ok=false; const auto frame=a[3].toLongLong(&ok); if(!ok)return fail(QStringLiteral("invalid frame")); ArtifactPr::EditorEngine::instance()->setOutPoint(frame); return 0; }


int listClips() { if(int r=requireProject()) return r; const auto s=ArtifactPr::EditorEngine::instance()->currentSequence(); QTextStream o(stdout); for(const auto& t:s.videoTracks) for(const auto& c:t.clips) o<<c.id<<'\t'<<t.id<<'\t'<<c.name<<'\t'<<c.startFrame<<'\t'<<c.duration<<'\n'; for(const auto& t:s.audioTracks) for(const auto& c:t.clips) o<<c.id<<'\t'<<t.id<<'\t'<<c.name<<'\t'<<c.startFrame<<'\t'<<c.duration<<'\n'; return 0; }
int runExport(const QStringList& a)
{
    if (int r = requireProject()) return r;
    if (a.size() < 2) return fail(QStringLiteral("export requires an output path"));

    auto* engine = ArtifactPr::EditorEngine::instance();
    qint64 start = 0;
    qint64 end = qMax<qint64>(0, engine->currentSequence().duration - 1);
    bool includeAudio = true;
    for (int i = 2; i < a.size(); ++i) {
        bool ok = false;
        if (a[i] == QStringLiteral("--start") && i + 1 < a.size()) {
            start = a[++i].toLongLong(&ok);
            if (!ok) return fail(QStringLiteral("invalid --start value"));
        } else if (a[i] == QStringLiteral("--end") && i + 1 < a.size()) {
            end = a[++i].toLongLong(&ok);
            if (!ok) return fail(QStringLiteral("invalid --end value"));
        } else if (a[i] == QStringLiteral("--no-audio")) {
            includeAudio = false;
        } else {
            return fail(QStringLiteral("unknown export option: ") + a[i]);
        }
    }
    if (start < 0 || end < start || end >= engine->currentSequence().duration) {
        return fail(QStringLiteral("export range is outside the sequence"));
    }
    if (engine->isExporting()) return fail(QStringLiteral("an export is already running"));

    const auto plan = engine->createRenderPlan(
        ArtifactPr::RenderQualityPreset::Full, start, end);
    if (!plan.isValid()) return fail(QStringLiteral("failed to create render plan"));

    ArtifactPr::ExportSettings settings;
    settings.outputPath = a[1];
    settings.includeAudio = includeAudio;

    bool done = false;
    bool success = false;
    QString message;
    auto* app = QCoreApplication::instance();
    QObject::connect(engine, &ArtifactPr::EditorEngine::exportFinished, app,
                     [&](bool ok, const QString& text) {
                         done = true;
                         success = ok;
                         message = text;
                         app->quit();
                     });

    engine->startExport(plan, settings);
    if (!done) app->exec();
    return success ? 0 : fail(message.isEmpty() ? QStringLiteral("export failed") : message);
}
}
int runArtifactPrCli(const QStringList& a) { if(a.size()<2){printUsage();return 0;} const auto c=a[1]; if(c==QStringLiteral("--help")||c==QStringLiteral("help")){printUsage();return 0;} if(c==QStringLiteral("--version")){QTextStream(stdout)<<"ArtifactPr 0.1\n";return 0;} if(c==QStringLiteral("project")){if(a.size()<3)return fail(QStringLiteral("project requires a subcommand")); if(a[2]==QStringLiteral("info"))return listProjectInfo(); if(a[2]==QStringLiteral("create")&&a.size()>=4)return createProject(a); if(a[2]==QStringLiteral("open")&&a.size()>=4)return ArtifactPr::EditorEngine::instance()->loadProject(a[3])?0:1; if(a[2]==QStringLiteral("save")&&a.size()>=4)return ArtifactPr::EditorEngine::instance()->saveProject(a[3])?0:1; return fail(QStringLiteral("unknown project subcommand")); } if(c==QStringLiteral("media")){if(a.size()>=3&&a[2]==QStringLiteral("list"))return listMedia();if(a.size()>=4&&a[2]==QStringLiteral("add"))return addMedia(a);return fail(QStringLiteral("unknown media subcommand"));} if(c==QStringLiteral("clip")){if(a.size()>=3&&a[2]==QStringLiteral("list"))return listClips();if(a.size()>=4&&a[2]==QStringLiteral("select"))return selectClip(a);if(a.size()>=5&&a[2]==QStringLiteral("move"))return moveClip(a);if(a.size()>=8&&a[2]==QStringLiteral("trim"))return trimClip(a);if(a.size()>=4&&a[2]==QStringLiteral("delete"))return deleteClip(a);if(a.size()>=4&&a[2]==QStringLiteral("ripple-delete"))return rippleDeleteClip(a);if(a.size()>=5&&a[2]==QStringLiteral("split"))return splitClip(a);return fail(QStringLiteral("unknown clip subcommand"));} if(c==QStringLiteral("sequence")){if(a.size()>=3&&a[2]==QStringLiteral("list"))return listSequences();if(a.size()>=4&&a[2]==QStringLiteral("select"))return ArtifactPr::EditorEngine::instance()->selectSequence(a[3])?0:fail(QStringLiteral("sequence not found"));return fail(QStringLiteral("unknown sequence subcommand"));} if(c==QStringLiteral("clip")&&a.size()>=3&&a[2]==QStringLiteral("list"))return listClips(); if(c==QStringLiteral("undo")||c==QStringLiteral("redo")){if(int r=requireProject())return r; c==QStringLiteral("undo")?ArtifactPr::EditorEngine::instance()->undo():ArtifactPr::EditorEngine::instance()->redo();return 0;} if(c==QStringLiteral("export"))return runExport(a); return fail(QStringLiteral("unknown command: ")+c); }