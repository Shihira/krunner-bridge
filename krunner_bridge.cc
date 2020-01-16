#include <iostream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>
#include <QThread>
#include <QtCore/QDir>

#include "krunner_bridge.h"

#define KB_ASSERT(cond) {if(!(cond)) {qDebug().nospace() << "[" << script << "] Failed on " << #cond; return;}}
#define KB_ASSERT_MSG(cond, msg) {if(!(cond)) {qDebug().nospace() << "[" << script << "] " << msg; return;}}

KRunnerBridge::KRunnerBridge(QObject *parent, const QVariantList &args)
        : Plasma::AbstractRunner(parent, args) {
    cwd = QDir::cleanPath(QDir::homePath() + QDir::separator() + ".local/share/kservices5");
    if (!QDir(cwd).exists()) cwd = "";

    // Read config from file
    for (const QString &kw : metadata().service()->propertyNames()) {
        if (kw.startsWith("X-KRunner-Bridge-Script")) {
            QString script = metadata().service()->property(kw, QVariant::String).toString();
            // find the corresponding script in desktop file install path
            const QString abs_script = QDir::cleanPath(cwd + QDir::separator() + script);
            if (QFileInfo(abs_script).exists()) script = abs_script;
            scripts.append(script);
        } else if (kw.startsWith("X-KRunner-Bridge-Run-Timeout")) {
            runTimeout = metadata().service()->property(kw, QVariant::String).toInt();
            if (runTimeout <= 0) runTimeout = 2000;
        } else if (kw.startsWith("X-KRunner-Bridge-Match-Timeout")) {
            matchTimeout = metadata().service()->property(kw, QVariant::String).toInt();
            if (matchTimeout <= 0) matchTimeout = 1000;
        }
    }

    setSpeed(AbstractRunner::NormalSpeed);
    setPriority(HighestPriority);
    setHasRunOptions(true);

    setDefaultSyntax(Plasma::RunnerSyntax(
            QString::fromLatin1(":q:"), metadata().comment()));

    const QByteArray json_init = R"({"operation":"init"})";
    QList<QProcess *> processes;
    for (const QString &script : scripts) {
        auto *process = new QProcess();
        process->setWorkingDirectory(cwd);
        process->start("python3", QStringList() << script << json_init);

        process->write(json_init);
        process->closeWriteChannel();
        processes.append(process);
    }
    for (auto *process:processes) {
        process->waitForFinished(runTimeout);
        process->deleteLater();
    }
}

void KRunnerBridge::match(Plasma::RunnerContext &ctxt) {
    if (!ctxt.isValid()) return;

    const QString query = ctxt.query();
    QJsonObject command;
    command.insert("operation", QJsonValue("match"));
    command.insert("query", QJsonValue(query));
    const QByteArray input = QJsonDocument(command).toJson(QJsonDocument::Compact);

    // Start all scripts parallel
    QList<QProcess *> processes;
    for (const QString &script : scripts) {
        auto *process = new QProcess();
        process->setWorkingDirectory(cwd);
        process->start("python3", QStringList() << script << input);
        processes.append(process);
    }

    // Evaluate output of the scripts
    // TODO Solve using signals
    for (auto *process:processes) {
        const QString script = process->program();
        KB_ASSERT_MSG(process->waitForFinished(matchTimeout), "Result retrieve timeout")

        // Retrieve output
        if (QProcess::CrashExit == process->exitStatus())
            qDebug() << process->readAllStandardError().data();

        const QByteArray output = process->readAllStandardOutput();
        process->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(output);

        KB_ASSERT(doc.isObject())
        QJsonValue v_result = doc.object().value("result");
        KB_ASSERT(v_result.isArray())

        for (const auto &result :v_result.toArray()) {
            KB_ASSERT(result.isObject());
            QJsonObject obj = result.toObject();

            Plasma::QueryMatch m(this);

            // Macro generates code that checks if the object has the key and writes it in the match
#define MAP_PROPERTY(t, camel1, camel2) \
            QJsonValue camel2 = obj.value(#camel2); \
            if(camel2.is##t()) m.set##camel1(camel2.to##t());

            MAP_PROPERTY(Double, Relevance, relevance);
            MAP_PROPERTY(String, Text, text);
            MAP_PROPERTY(String, Subtext, subtext);
            MAP_PROPERTY(String, MatchCategory, matchCategory);
            MAP_PROPERTY(String, IconName, iconName);
            MAP_PROPERTY(String, MimeType, mimeType);

#undef MAP_PROPERTY

            QJsonValue data = obj.value("data");
            if (!data.isUndefined()) m.setData(QVariantList({script, data.toVariant()}));
            else m.setData(QVariantList());

            m.setType(relevance == 1 ? Plasma::QueryMatch::ExactMatch : Plasma::QueryMatch::CompletionMatch);
            ctxt.addMatch(m);
        }
    }
}

void KRunnerBridge::run(const Plasma::RunnerContext &ctxt, const Plasma::QueryMatch &match) {
    Q_UNUSED(ctxt)

    const QVariantList data = match.data().toList();
    if (data.size() != 2) return;

    // Data should only be passed to script that generated the run option
    const QString script = data.at(0).toString();
    QJsonObject command;
    command.insert("operation", QJsonValue("run"));
    command.insert("data", QJsonValue::fromVariant(data.at(1)));

    QProcess process;
    process.setWorkingDirectory(cwd);
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start("sh", QStringList() << "-c" << script);

    const QByteArray input = QJsonDocument(command).toJson(QJsonDocument::Compact);
    process.write(input);
    process.closeWriteChannel();

    KB_ASSERT_MSG(process.waitForFinished(runTimeout), "Running timeout");
}

K_EXPORT_PLASMA_RUNNER(krunner_bridge, KRunnerBridge)

#include "krunner_bridge.moc"
