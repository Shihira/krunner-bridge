#include <iostream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>
#include <QtCore/QDir>

#include "krunner_bridge.h"

#define KB_ASSERT(cond) {if(!(cond)) {qDebug().nospace() << "[" << script << "] Failed on " << #cond; return;}}
#define KB_ASSERT_MSG(cond, msg) {if(!(cond)) {qDebug().nospace() << "[" << script << "] " << msg; return;}}

KRunnerBridge::KRunnerBridge(QObject* parent, const QVariantList& args)
  : Plasma::AbstractRunner(parent, args)
{
    cwd = QDir::cleanPath(QDir::homePath() + QDir::separator() + ".local/share/kservices5");
    if(!QDir(cwd).exists()) cwd = "";

    for(const QString& kw : metadata().service()->propertyNames()) {
        if(kw.startsWith("X-KRunner-Bridge-Script")) {
            QString script = metadata().service()->property(kw, QVariant::String).toString();
            // find the corresponding script in desktop file install path
            QString abs_script = QDir::cleanPath(cwd + QDir::separator() + script);
            if(QFileInfo(abs_script).exists()) script = abs_script;

            scripts.append(script);
        }
    }

    setSpeed(AbstractRunner::NormalSpeed);
    setPriority(HighestPriority);
    setHasRunOptions(true);

    setDefaultSyntax(Plasma::RunnerSyntax(
        QString::fromLatin1(":q:"), metadata().comment()));

    for(QString& script : scripts) {
        QProcess process;
        process.setWorkingDirectory(cwd);
        process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        process.start("sh", QStringList() << "-c" << script);

        QJsonObject command;
        command.insert("operation", QJsonValue("init"));

        QByteArray input = QJsonDocument(command).toJson(QJsonDocument::Compact);
        process.write(input);
        process.closeWriteChannel();

        KB_ASSERT_MSG(process.waitForFinished(1000), "Initialization timeout");
    }
}

void KRunnerBridge::match(Plasma::RunnerContext& ctxt)
{
    if (!ctxt.isValid()) return;

    for(QString& script : scripts) {
        QProcess process;
        process.setWorkingDirectory(cwd);
        process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        process.start("sh", QStringList() << "-c" << script);

        //// Prepare for input
        QString query = ctxt.query();

        QJsonObject command;
        command.insert("operation", QJsonValue("match"));
        command.insert("query", QJsonValue(query));

        QByteArray input = QJsonDocument(command).toJson(QJsonDocument::Compact);
        process.write(input);
        process.closeWriteChannel();

        KB_ASSERT_MSG(process.waitForFinished(1000), "Result retrieve timeout");

        //// Retrieve output
        if(process.exitStatus() == QProcess::CrashExit)
            qDebug() << process.readAllStandardError().data();

        QByteArray output = process.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(output);

        KB_ASSERT(doc.isObject());
        QJsonValue v_result = doc.object().value("result");
        KB_ASSERT(v_result.isArray());
        QJsonArray results = v_result.toArray();

        for(QJsonValue result : results) {
            KB_ASSERT(result.isObject());
            QJsonObject obj = result.toObject();

            Plasma::QueryMatch m(this);

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
            if(!data.isUndefined()) m.setData(data.toVariant());

            if(relevance == 1)
                m.setType(Plasma::QueryMatch::ExactMatch);
            else m.setType(Plasma::QueryMatch::CompletionMatch);

            ctxt.addMatch(m);
        }
    }
}

void KRunnerBridge::run(const Plasma::RunnerContext& ctxt, const Plasma::QueryMatch& match)
{
    for(QString& script : scripts) {
        QProcess process;
        process.setWorkingDirectory(cwd);
        process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        process.start("sh", QStringList() << "-c" << script);

        QJsonObject command;
        command.insert("operation", QJsonValue("run"));
        command.insert("data", QJsonValue::fromVariant(match.data()));

        QByteArray input = QJsonDocument(command).toJson(QJsonDocument::Compact);
        process.write(input);
        process.closeWriteChannel();

        KB_ASSERT_MSG(process.waitForFinished(2000), "Running timeout");
    }
}

K_EXPORT_PLASMA_RUNNER(krunner_bridge, KRunnerBridge)

#include "krunner_bridge.moc"
