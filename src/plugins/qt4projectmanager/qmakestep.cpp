/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "qmakestep.h"

#include "qt4project.h"
#include "qt4projectmanagerconstants.h"
#include "qt4projectmanager.h"
#include "makestep.h"
#include "qtversionmanager.h"
#include "qt4buildconfiguration.h"

#include <coreplugin/icore.h>
#include <utils/qtcassert.h>

#include <projectexplorer/projectexplorerconstants.h>

#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QCoreApplication>

using namespace Qt4ProjectManager;
using namespace Qt4ProjectManager::Internal;
using namespace ProjectExplorer;

QMakeStep::QMakeStep(ProjectExplorer::BuildConfiguration *bc)
    : AbstractMakeStep(bc), m_forced(false)
{
}

QMakeStep::QMakeStep(QMakeStep *bs, ProjectExplorer::BuildConfiguration *bc)
    : AbstractMakeStep(bs, bc),
    m_forced(false),
    m_qmakeArgs(bs->m_qmakeArgs)
{

}

QMakeStep::~QMakeStep()
{
}

QStringList QMakeStep::arguments()
{
    QStringList additonalArguments = m_qmakeArgs;
    Qt4BuildConfiguration *bc = static_cast<Qt4BuildConfiguration *>(buildConfiguration());
    QStringList arguments;
    arguments << buildConfiguration()->project()->file()->fileName();
    arguments << "-r";

    // TODO
    if (!additonalArguments.contains("-spec"))
        arguments << "-spec" << bc->qtVersion()->mkspec();

#ifdef Q_OS_WIN
    ToolChain::ToolChainType type = pro->toolChainType(bc);
    if (type == ToolChain::GCC_MAEMO)
        arguments << QLatin1String("-unix");
#endif

    if (bc->value("buildConfiguration").isValid()) {
        QStringList configarguments;
        QtVersion::QmakeBuildConfigs defaultBuildConfiguration = bc->qtVersion()->defaultBuildConfig();
        QtVersion::QmakeBuildConfigs projectBuildConfiguration = QtVersion::QmakeBuildConfig(bc->value("buildConfiguration").toInt());
        if ((defaultBuildConfiguration & QtVersion::BuildAll) && !(projectBuildConfiguration & QtVersion::BuildAll))
            configarguments << "CONFIG-=debug_and_release";
        if (!(defaultBuildConfiguration & QtVersion::BuildAll) && (projectBuildConfiguration & QtVersion::BuildAll))
            configarguments << "CONFIG+=debug_and_release";
        if ((defaultBuildConfiguration & QtVersion::DebugBuild) && !(projectBuildConfiguration & QtVersion::DebugBuild))
            configarguments << "CONFIG+=release";
        if (!(defaultBuildConfiguration & QtVersion::DebugBuild) && (projectBuildConfiguration & QtVersion::DebugBuild))
            configarguments << "CONFIG+=debug";
        if (!configarguments.isEmpty())
            arguments << configarguments;
    } else {
        qWarning()<< "The project should always have a qmake build configuration set";
    }

    if (!additonalArguments.isEmpty())
        arguments << additonalArguments;

    return arguments;
}

bool QMakeStep::init()
{
    // TODO
    Qt4BuildConfiguration *qt4bc = static_cast<Qt4BuildConfiguration *>(buildConfiguration());
    const QtVersion *qtVersion = qt4bc->qtVersion();

    if (!qtVersion->isValid()) {
#if defined(Q_WS_MAC)
        emit addToOutputWindow(tr("\n<font color=\"#ff0000\"><b>No valid Qt version set. Set one in Preferences </b></font>\n"));
#else
        emit addToOutputWindow(tr("\n<font color=\"#ff0000\"><b>No valid Qt version set. Set one in Tools/Options </b></font>\n"));
#endif
        return false;
    }

    QStringList args = arguments();
    QString workingDirectory = qt4bc->buildDirectory();

    QString program = qtVersion->qmakeCommand();

    // Check wheter we need to run qmake
    m_needToRunQMake = true;
    if (QDir(workingDirectory).exists(QLatin1String("Makefile"))) {
        QString qmakePath = QtVersionManager::findQMakeBinaryFromMakefile(workingDirectory);
        if (qtVersion->qmakeCommand() == qmakePath) {
            m_needToRunQMake = !qt4bc->compareBuildConfigurationToImportFrom(workingDirectory);
        }
    }

    if (m_forced) {
        m_forced = false;
        m_needToRunQMake = true;
    }

    setEnabled(m_needToRunQMake);
    setWorkingDirectory(workingDirectory);
    setCommand(program);
    setArguments(args);
    setEnvironment(qt4bc->environment());

    setBuildParser(ProjectExplorer::Constants::BUILD_PARSER_QMAKE);
    return AbstractMakeStep::init();
}

void QMakeStep::run(QFutureInterface<bool> &fi)
{
    //TODO
    Qt4Project *pro = static_cast<Qt4Project *>(buildConfiguration()->project());
    if (pro->rootProjectNode()->projectType() == ScriptTemplate) {
        fi.reportResult(true);
        return;
    }

    if (!m_needToRunQMake) {
        emit addToOutputWindow(tr("<font color=\"#0000ff\">Configuration unchanged, skipping QMake step.</font>"));
        fi.reportResult(true);
        return;
    }
    AbstractMakeStep::run(fi);
}

QString QMakeStep::name()
{
    return Constants::QMAKESTEP;
}

QString QMakeStep::displayName()
{
    return "QMake";
}

void QMakeStep::setForced(bool b)
{
    m_forced = b;
}

bool QMakeStep::forced()
{
    return m_forced;
}

ProjectExplorer::BuildStepConfigWidget *QMakeStep::createConfigWidget()
{
    return new QMakeStepConfigWidget(this);
}

bool QMakeStep::immutable() const
{
    return false;
}

void QMakeStep::processStartupFailed()
{
    m_forced = true;
    AbstractProcessStep::processStartupFailed();
}

bool QMakeStep::processFinished(int exitCode, QProcess::ExitStatus status)
{
    bool result = AbstractProcessStep::processFinished(exitCode, status);
    if (!result)
        m_forced = true;
    return result;
}

void QMakeStep::setQMakeArguments(const QStringList &arguments)
{
    m_qmakeArgs = arguments;
    emit changed();
}

QStringList QMakeStep::qmakeArguments()
{
    return m_qmakeArgs;
}

void QMakeStep::restoreFromLocalMap(const QMap<QString, QVariant> &map)
{
    m_qmakeArgs = map.value("qmakeArgs").toStringList();
    AbstractProcessStep::restoreFromLocalMap(map);
}

void QMakeStep::storeIntoLocalMap(QMap<QString, QVariant> &map)
{
    map["qmakeArgs"] = m_qmakeArgs;
    AbstractProcessStep::storeIntoLocalMap(map);
}

QMakeStepConfigWidget::QMakeStepConfigWidget(QMakeStep *step)
    : BuildStepConfigWidget(), m_step(step)
{
    m_ui.setupUi(this);
    connect(m_ui.qmakeAdditonalArgumentsLineEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(qmakeArgumentsLineEditTextEdited()));
    connect(m_ui.buildConfigurationComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(buildConfigurationChanged()));
    connect(step, SIGNAL(changed()),
            this, SLOT(update()));
    connect(step->buildConfiguration(), SIGNAL(qtVersionChanged(ProjectExplorer::BuildConfiguration *)),
            this, SLOT(qtVersionChanged(ProjectExplorer::BuildConfiguration *)));
}

QString QMakeStepConfigWidget::summaryText() const
{
    return m_summaryText;
}

void QMakeStepConfigWidget::qtVersionChanged(ProjectExplorer::BuildConfiguration *bc)
{
    if (bc == m_step->buildConfiguration()) {
        updateTitleLabel();
        updateEffectiveQMakeCall();
    }
}

void QMakeStepConfigWidget::updateTitleLabel()
{
    Qt4BuildConfiguration *qt4bc = static_cast<Qt4BuildConfiguration *>(m_step->buildConfiguration());
    const QtVersion *qtVersion = qt4bc->qtVersion();
    if (!qtVersion) {
        m_summaryText = tr("<b>QMake:</b> No Qt version set. QMake can not be run.");
        emit updateSummary();
        return;
    }

    QStringList args = m_step->arguments();
    // We don't want the full path to the .pro file
    const QString projectFileName = m_step->buildConfiguration()->project()->file()->fileName();
    int index = args.indexOf(projectFileName);
    if (index != -1)
        args[index] = QFileInfo(projectFileName).fileName();

    // And we only use the .pro filename not the full path
    QString program = QFileInfo(qtVersion->qmakeCommand()).fileName();
    m_summaryText = tr("<b>QMake:</b> %1 %2").arg(program, args.join(QString(QLatin1Char(' '))));
    emit updateSummary();

}

void QMakeStepConfigWidget::qmakeArgumentsLineEditTextEdited()
{
    m_step->setQMakeArguments(
            ProjectExplorer::Environment::parseCombinedArgString(m_ui.qmakeAdditonalArgumentsLineEdit->text()));

    static_cast<Qt4Project *>(m_step->buildConfiguration()->project())->invalidateCachedTargetInformation();
    updateTitleLabel();
    updateEffectiveQMakeCall();
}

void QMakeStepConfigWidget::buildConfigurationChanged()
{
    ProjectExplorer::BuildConfiguration *bc = m_step->buildConfiguration();
    QtVersion::QmakeBuildConfigs buildConfiguration = QtVersion::QmakeBuildConfig(bc->value("buildConfiguration").toInt());
    if (m_ui.buildConfigurationComboBox->currentIndex() == 0) {
        // debug
        buildConfiguration = buildConfiguration | QtVersion::DebugBuild;
    } else {
        buildConfiguration = buildConfiguration & ~QtVersion::DebugBuild;
    }
    bc->setValue("buildConfiguration", int(buildConfiguration));
    static_cast<Qt4Project *>(m_step->buildConfiguration()->project())->invalidateCachedTargetInformation();
    updateTitleLabel();
    updateEffectiveQMakeCall();
    // TODO if exact parsing is the default, we need to update the code model
    // and all the Qt4ProFileNodes
    //static_cast<Qt4Project *>(m_step->project())->update();
}

QString QMakeStepConfigWidget::displayName() const
{
    return m_step->displayName();
}

void QMakeStepConfigWidget::update()
{
    init();
}

void QMakeStepConfigWidget::init()
{
    QString qmakeArgs = ProjectExplorer::Environment::joinArgumentList(m_step->qmakeArguments());
    m_ui.qmakeAdditonalArgumentsLineEdit->setText(qmakeArgs);
    ProjectExplorer::BuildConfiguration *bc = m_step->buildConfiguration();
    bool debug = QtVersion::QmakeBuildConfig(bc->value("buildConfiguration").toInt()) & QtVersion::DebugBuild;
    m_ui.buildConfigurationComboBox->setCurrentIndex(debug? 0 : 1);
    updateTitleLabel();
    updateEffectiveQMakeCall();
}

void QMakeStepConfigWidget::updateEffectiveQMakeCall()
{
    Qt4BuildConfiguration *qt4bc = static_cast<Qt4BuildConfiguration *>(m_step->buildConfiguration());
    const QtVersion *qtVersion = qt4bc->qtVersion();
    if (qtVersion) {
        QString program = QFileInfo(qtVersion->qmakeCommand()).fileName();
        m_ui.qmakeArgumentsEdit->setPlainText(program + QLatin1Char(' ') + ProjectExplorer::Environment::joinArgumentList(m_step->arguments()));
    } else {
        m_ui.qmakeArgumentsEdit->setPlainText(tr("No valid Qt version set."));
    }
}

////
// QMakeStepFactory
////

QMakeStepFactory::QMakeStepFactory()
{
}

QMakeStepFactory::~QMakeStepFactory()
{
}

bool QMakeStepFactory::canCreate(const QString & name) const
{
    return (name == Constants::QMAKESTEP);
}

ProjectExplorer::BuildStep *QMakeStepFactory::create(BuildConfiguration *bc, const QString & name) const
{
    Q_UNUSED(name)
    return new QMakeStep(bc);
}

ProjectExplorer::BuildStep *QMakeStepFactory::clone(ProjectExplorer::BuildStep *bs, ProjectExplorer::BuildConfiguration *bc) const
{
    return new QMakeStep(static_cast<QMakeStep *>(bs), bc);
}

QStringList QMakeStepFactory::canCreateForBuildConfiguration(ProjectExplorer::BuildConfiguration *bc) const
{
    if (qobject_cast<Qt4BuildConfiguration *>(bc))
        return QStringList() << Constants::QMAKESTEP;
    return QStringList();
}

QString QMakeStepFactory::displayNameForName(const QString &name) const
{
    Q_UNUSED(name);
    return tr("QMake");
}


