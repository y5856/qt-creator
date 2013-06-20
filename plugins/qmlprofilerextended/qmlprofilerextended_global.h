/****************************************************************************
**
** Copyright (C) 2013 Digia Plc
** All rights reserved.
** For any questions to Digia, please use contact form at http://qt.digia.com <http://qt.digia.com/>
**
** This file is part of the Qt Enterprise Qt Quick Profiler Add-on.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.digia.com <http://qt.digia.com/>
**
****************************************************************************/

#ifndef QMLPROFILEREXTENDED_GLOBAL_H
#define QMLPROFILEREXTENDED_GLOBAL_H

#include <QtGlobal>

#if defined(QMLPROFILEREXTENDED_LIBRARY)
#  define QMLPROFILEREXTENDEDSHARED_EXPORT Q_DECL_EXPORT
#else
#  define QMLPROFILEREXTENDEDSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // QMLPROFILEREXTENDED_GLOBAL_H

