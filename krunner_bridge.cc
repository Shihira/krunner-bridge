#include <iostream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>
#include <QThread>
#include <QtCore/QDir>
#include <KConfigCore/KSharedConfig>
#include <QStringBuilder>

#include "krunner_bridge.h"

#define KB_ASSERT(cond) {if(!(cond)) {qDebug().nospace() << "[" << script << "] Failed on " << #cond << "JSON value was: \n" << output; continue;}}
#define KB_ASSERT_MSG(cond, msg) {if(!(cond)) {qDebug().nospace() << "[" << script << "] " << msg; continue;}}

KRunnerBridge::KRunnerBridge(QObject *parent, const QVariantList &args)
        : Plasma::AbstractRunner(parent, args) {

    setSpeed(AbstractRunner::NormalSpeed);
    setPriority(HighestPriority);
    setHasRunOptions(true);
    setDefaultSyntax(Plasma::RunnerSyntax(QStringLiteral(":q:"), metadata().comment()));

    if (!QDir(cwd).exists()) {
        cwd = QDir::homePath();
    }

    initializeValues();
    parseKRunnerConfigLines();
    parseKRunnerConfigGroups();

    QList<QProcess *> processes;
    for (const KRunnerScript &script : qAsConst(scripts)) {
        processes.append(script.startProcess(json_init, cwd));
    }
    for (auto *process : qAsConst(processes)) {
        process->waitForFinished(initTimeout);
        process->deleteLater();
    }

}

void KRunnerBridge::initializeValues() {
    initTimeout = metadata().service()->property(QStringLiteral("X-KRunner-Bridge-Init-Timeout"), QVariant::Int).toInt();
    setupTimeout = metadata().service()->property(QStringLiteral("X-KRunner-Bridge-Setup-Timeout"), QVariant::Int).toInt();
    teardownTimeout = metadata().service()->property(QStringLiteral("X-KRunner-Bridge-Teardown-Timeout"), QVariant::Int).toInt();
    runTimeout = metadata().service()->property(QStringLiteral("X-KRunner-Bridge-Run-Timeout"), QVariant::Int).toInt();
    matchTimeout = metadata().service()->property(QStringLiteral("X-KRunner-Bridge-Match-Timeout"), QVariant::Int).toInt();
    debugOutput = metadata().service()->property(QStringLiteral("X-KRunner-Bridge-Debug"), QVariant::Bool).toBool();
}

void KRunnerBridge::parseKRunnerConfigLines() {
    const QLatin1String krunnerBridgeScriptKey = QLatin1String("X-KRunner-Bridge-Script");
    const auto keyWords = metadata().service()->propertyNames();
    for (const QString &kw : keyWords) {
        if (kw.startsWith(krunnerBridgeScriptKey)) {
            QString script = metadata().service()->property(kw, QVariant::String).toString();
            // find the corresponding script in desktop file install path
            if (!QFile::exists(script)) {
                const QString abs_script = QDir::cleanPath(cwd + QDir::separator() + script);
                if (QFileInfo(abs_script).exists()) {
                    script = abs_script;
                } else {
                    qWarning() << "Script " << script << " is not a file";
                    continue;
                }
            }
            KRunnerScript kRunnerScript;
            kRunnerScript.filePath = script;
            scripts.append(kRunnerScript);
        }
    }
}

void KRunnerBridge::parseKRunnerConfigGroups() {
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QDir::homePath()
                                                          % QDir::separator()
                                                          % QStringLiteral(".local/share/kservices5/krunner_bridge.desktop"));
    bool hasPrepareScript = false;
    bool hasTeardownScript = false;

    const auto entries = config->groupList();
    const QLatin1String krunnerBridgeScriptKey = QLatin1String("X-KRunner-Bridge-Script");
    for (const QString &groupName : entries) {
        if (groupName.startsWith(krunnerBridgeScriptKey)) {
            const KConfigGroup group = config->group(groupName);
            KRunnerScript script;
            script.matchRegex = QRegularExpression(group.readEntry("MatchRegex", ".*"));
            script.launchCommand = group.readEntry("LaunchCommand", "python3");
            script.filePath = group.readEntry("FilePath");
            if (script.filePath.isEmpty()) {
                qWarning() << "No FilePath for " << groupName << " provided";
                continue;
            }
            script.initialize = group.readEntry("Initialize", true);
            script.prepare = group.readEntry("Prepare", true);
            if (script.prepare) {
                hasPrepareScript = true;
            }
            script.teardown = group.readEntry("Teardown", true);
            if (script.teardown) {
                hasTeardownScript = true;
            }
            script.runDetatched = group.readEntry("RunDetached ", true);
            scripts.append(script);
        }
    }

    // Connect only if at least one script requires the signal
    if (hasPrepareScript) {
        connect(this, &KRunnerBridge::prepare, this, &KRunnerBridge::prepareForMatchSession);
    }
    if (hasTeardownScript) {
        connect(this, &KRunnerBridge::teardown, this, &KRunnerBridge::matchSessionFinished);
    }
}

void KRunnerBridge::prepareForMatchSession() {
    QList<QProcess *> processes;
    for (const KRunnerScript &script : qAsConst(scripts)) {
        if (script.prepare) {
            processes.append(script.startProcess(json_prepare, cwd));
        }
    }
    for (auto *process : qAsConst(processes)) {
        process->waitForFinished(setupTimeout);
        process->deleteLater();
    }
}

void KRunnerBridge::matchSessionFinished() {
    QList<QProcess *> processes;
    for (const KRunnerScript &script : qAsConst(scripts)) {
        if (script.prepare) {
            processes.append(script.startProcess(json_teardown, cwd));
        }
    }
    for (auto *process : qAsConst(processes)) {
        process->waitForFinished(teardownTimeout);
        process->deleteLater();
    }
}

void KRunnerBridge::match(Plasma::RunnerContext &ctxt) {
    if (!ctxt.isValid()) {
        return;
    }
    const QString query = ctxt.query();
    QJsonObject command;
    command.insert("operation", QJsonValue("match"));
    command.insert("query", QJsonValue(query));
    const QByteArray input = QJsonDocument(command).toJson(QJsonDocument::Compact);

    // Start all scripts parallel
    QList<QProcess *> processes;
    for (const KRunnerScript &script : qAsConst(scripts)) {
        if (script.matchRegex.match(query).hasMatch()) {
            processes.append(script.startProcess(input, cwd));
        }
    }

    // Evaluate output of the scripts
    // TODO Solve using signals
    for (int i = 0, processCount = processes.count(); i < processCount; ++i) {
        // Kill processes if context is not valid anymore
        if (!ctxt.isValid()) {
            for (auto p: qAsConst(processes)) {
                p->kill();
                p->deleteLater();
            }
            return;
        }

        auto process = processes.at(i);
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

        if (debugOutput) {
            qInfo() << "Debug output for: " << script << " " << process->arguments().at(0);
            qInfo().noquote() << doc.toJson(QJsonDocument::JsonFormat::Indented);
        }

        for (const auto &result :v_result.toArray()) {
            KB_ASSERT(result.isObject());
            const QJsonObject obj = result.toObject();

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
            if (!data.isUndefined()) {
                m.setData(QVariantList({i, data.toVariant()}));
            } else {
                m.setData(QVariantList());
            }

            m.setType(relevance == 1 ? Plasma::QueryMatch::ExactMatch : Plasma::QueryMatch::CompletionMatch);
            ctxt.addMatch(m);
        }
    }
    qDeleteAll(processes);
}

void KRunnerBridge::run(const Plasma::RunnerContext &ctxt, const Plasma::QueryMatch &match) {
    Q_UNUSED(ctxt)

    const QVariantList data = match.data().toList();

    const KRunnerScript &script = scripts.at(data.at(0).toInt());
    QJsonObject command;
    command.insert("operation", QJsonValue("run"));
    command.insert("data", QJsonValue::fromVariant(data.at(1)));

    if (script.runDetatched) {
        QProcess::startDetached(script.launchCommand,
                                QStringList() << script.filePath << QJsonDocument(command).toJson(QJsonDocument::Compact));
    } else {
        QProcess *p = script.startProcess(QJsonDocument(command).toJson(QJsonDocument::Compact), cwd);
        p->waitForFinished(runTimeout);
        p->deleteLater();
    }
}


K_EXPORT_PLASMA_RUNNER(krunner_bridge, KRunnerBridge)

#include "krunner_bridge.moc"
