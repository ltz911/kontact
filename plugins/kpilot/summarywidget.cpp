/*
    This file is part of Kontact.
    Copyright (c) 2003 Tobias Koenig <tokoe@kde.org>
    Copyright (C) 2004 Reinhold Kainhofer <reinhold@kainhofer.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
*/

#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qfile.h>
#include <qlabel.h>
#include <q3textedit.h>
#include <kvbox.h>
//Added by qt3to4:
#include <QPixmap>
#include <QGridLayout>
#include <QTextStream>
#include <Q3CString>

#include <dcopclient.h>
#include <dcopref.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kurllabel.h>
#include <kdialogbase.h>
#include <kmessagebox.h>

#include "pilotDaemonDCOP_stub.h"

#include <ktextedit.h>
#include <ktoolinvocation.h>

#include "summarywidget.h"

SummaryWidget::SummaryWidget( QWidget *parent, const char *name )
  : Kontact::Summary( parent, name ),
    DCOPObject( "KPilotSummaryWidget" ),
    mDCOPSuccess( false ),
    mStartedDaemon( false ),
    mShouldStopDaemon( true )
{
  mLayout = new QGridLayout( this );
  mLayout->setSpacing( 3 );
  mLayout->setMargin( 3 );

  int row=0;
  QPixmap icon = KGlobal::iconLoader()->loadIcon( "kpilot", K3Icon::Desktop, K3Icon::SizeMedium );
  QWidget *header = createHeader( this, icon, i18n( "KPilot Information" ) );
  mLayout->addWidget( header, row, 0, 1, 4 );

  // Last sync information
  row++;
  mSyncTimeTextLabel = new QLabel( i18n( "<i>Last sync:</i>" ), this);
  mLayout->addWidget( mSyncTimeTextLabel, row, 0 );
  mSyncTimeLabel = new QLabel( i18n( "No information available" ), this );
  mLayout->addWidget( mSyncTimeLabel, row, 1 );
  mShowSyncLogLabel = new KUrlLabel( "", i18n( "[View Sync Log]" ), this );
  mLayout->addWidget( mShowSyncLogLabel, row, 3 );
  connect( mShowSyncLogLabel, SIGNAL( leftClickedURL( const QString& ) ),
    this, SLOT( showSyncLog( const QString& ) ) );

  // User
  row++;
  mPilotUserTextLabel = new QLabel( i18n( "<i>User:</i>" ), this);
  mLayout->addWidget( mPilotUserTextLabel, row, 0);
  mPilotUserLabel = new QLabel( i18n( "Unknown" ), this );
  mLayout->addWidget( mPilotUserLabel, row, 1, 1, 3 );

  // Device information
  row++;
  mPilotDeviceTextLabel = new QLabel( i18n( "<i>Device:</i>" ), this);
  mLayout->addWidget( mPilotDeviceTextLabel, row, 0 );
  mPilotDeviceLabel = new QLabel( i18n( "Unknown" ), this );
  mLayout->addWidget( mPilotDeviceLabel, row, 1, 1, 3 );

  // Status
  row++;
  mDaemonStatusTextLabel = new QLabel( i18n( "<i>Status:</i>" ), this);
  mLayout->addWidget( mDaemonStatusTextLabel, row, 0 );
  mDaemonStatusLabel = new QLabel( i18n( "No communication with the daemon possible" ), this );
  mLayout->addWidget( mDaemonStatusLabel, row, 1, 1, 3 );

  // Conduits:
  row++;
  mConduitsTextLabel = new QLabel( i18n( "<i>Conduits:</i>" ), this );
  mConduitsTextLabel->setAlignment( Qt::AlignLeft | Qt::AlignTop | Qt::TextExpandTabs );
  mLayout->addWidget( mConduitsTextLabel, row, 0 );
  mConduitsLabel = new QLabel( i18n( "No information available" ), this );
  mConduitsLabel->setWordWrap( true );
  mLayout->addWidget( mConduitsLabel, row, 1, 1, 3 );

  // widgets shown if kpilotDaemon is not running
  row++;
  mNoConnectionLabel = new QLabel( i18n( "KPilot is currently not running." ), this );
  mLayout->addWidget( mNoConnectionLabel, row, 1, 1, 2 );
  mNoConnectionStartLabel = new KUrlLabel( "", i18n( "[Start KPilot]" ), this );
  mLayout->addWidget( mNoConnectionStartLabel, row, 3 );
  connect( mNoConnectionStartLabel, SIGNAL( leftClickedURL( const QString& ) ),
           this, SLOT( startKPilot() ) );

  if ( !kapp->dcopClient()->isApplicationRegistered( "kpilotDaemon" ) ) {
    startKPilot();
  }

  connectDCOPSignal( 0, 0, "kpilotDaemonStatusDetails(QDateTime,QString,QStringList,QString,QString,QString,bool)",
                     "receiveDaemonStatusDetails(QDateTime,QString,QStringList,QString,QString,QString,bool)", false );
	connect( kapp->dcopClient(), SIGNAL( applicationRemoved( const Q3CString & ) ), SLOT( slotAppRemoved( const Q3CString& ) ) );
}

SummaryWidget::~SummaryWidget()
{
  if ( mStartedDaemon && mShouldStopDaemon ) {
    PilotDaemonDCOP_stub dcopToDaemon( "kpilotDaemon", "KPilotDaemonIface" );
    dcopToDaemon.quitNow(); // ASYNC, always succeeds.
  }
}

QStringList SummaryWidget::configModules() const
{
  QStringList modules;
  modules << "kpilot_config.desktop";
  return modules;
}

void SummaryWidget::receiveDaemonStatusDetails(QDateTime lastSyncTime, QString status, QStringList conduits, QString logFileName, QString userName, QString pilotDevice, bool killOnExit )
{
  mDCOPSuccess = true;
  mLastSyncTime = lastSyncTime;
  mDaemonStatus = status;
  mConduits = conduits;
  mSyncLog = logFileName;
  mUserName = userName;
  mPilotDevice = pilotDevice;
  mShouldStopDaemon = killOnExit;
  updateView();
}

void SummaryWidget::updateView()
{
  if ( mDCOPSuccess ) {
    if ( mLastSyncTime.isValid() ) {
      mSyncTimeLabel->setText( mLastSyncTime.toString(Qt::LocalDate) );
    } else {
      mSyncTimeLabel->setText( i18n( "No information available" ) );
    }
    if ( !mSyncLog.isEmpty() ) {
      mShowSyncLogLabel->setEnabled( true );
      mShowSyncLogLabel->setURL( mSyncLog );
    } else {
      mShowSyncLogLabel->setEnabled( false );
    }
    mPilotUserLabel->setText( mUserName.isEmpty() ? i18n( "unknown" ) : mUserName );
    mPilotDeviceLabel->setText( mPilotDevice.isEmpty() ? i18n( "unknown" ) : mPilotDevice );
    mDaemonStatusLabel->setText( mDaemonStatus );
    mConduitsLabel->setText( mConduits.join( ", " ) );
  } else {
    mSyncTimeLabel->setText( i18n( "No information available (Daemon not running?)" ) );
    mShowSyncLogLabel->setEnabled( false );
    mPilotUserLabel->setText( i18n( "unknown" ) );
    mPilotDeviceLabel->setText( i18n( "unknown" ) );
    mDaemonStatusLabel->setText( i18n( "No communication with the daemon possible" ) );
    mConduitsLabel->setText( i18n( "No information available" ) );
  }

  mSyncTimeTextLabel->setShown( mDCOPSuccess );
  mSyncTimeLabel->setShown( mDCOPSuccess );
  mShowSyncLogLabel->setShown( mDCOPSuccess );
  mPilotUserTextLabel->setShown( mDCOPSuccess );
  mPilotUserLabel->setShown( mDCOPSuccess );
  mPilotDeviceTextLabel->setShown( mDCOPSuccess );
  mPilotDeviceLabel->setShown( mDCOPSuccess );
  mDaemonStatusTextLabel->setShown( mDCOPSuccess );
  mDaemonStatusLabel->setShown( mDCOPSuccess );
  mConduitsTextLabel->setShown( mDCOPSuccess );
  mConduitsLabel->setShown( mDCOPSuccess );
  mNoConnectionLabel->setShown( !mDCOPSuccess );
  mNoConnectionStartLabel->setShown( !mDCOPSuccess );
}

void SummaryWidget::showSyncLog( const QString &filename )
{
  KDialogBase dlg( this, 0, true, QString::null, KDialogBase::Ok, KDialogBase::Ok );
  dlg.setCaption( i18n( "KPilot HotSync Log" ) );

  Q3TextEdit *edit = new Q3TextEdit( dlg.makeVBoxMainWidget() );
  edit->setReadOnly( true );

  QFile f(filename);
  if ( !f.open( QIODevice::ReadOnly ) ) {
    KMessageBox::error( this, i18n( "Unable to open Hotsync log %1.", filename ) );
    return;
  }

  QTextStream s( &f );
  while ( !s.atEnd() )
    edit->append( s.readLine() );

  edit->moveCursor( Q3TextEdit::MoveHome, false );

  f.close();

  dlg.setInitialSize( QSize( 400, 350 ) );
  dlg.exec();
}

void SummaryWidget::startKPilot()
{
  QString error;
  Q3CString appID;
  QStringList lst;
  lst << "--fail-silently";
  if ( !KToolInvocation::kdeinitExec( "kpilotDaemon", /*QString( "--fail-silently" )*/lst ) ) {
    kDebug(5602) << "No service available..." << endl;
    mStartedDaemon = true;
  }
}

void SummaryWidget::slotAppRemoved( const Q3CString & appId )
{
  if ( appId == "kpilotDaemon" )
  {
    mDCOPSuccess = false;
    updateView();
  }
}


#include "summarywidget.moc"

