/*
 * Transfer widgets and handler
 */
#ifndef H_TRANSFER
#define H_TRANSFER

#include "network.h"
#include "common.h"
#include "miscWidgets.h"

#include <QtGui>
#include <QList>

class TransferWidget;

class InTransferWidget;
class OutTransferWidget;

/*
 * Transfer widget
 */

class TransferWidget : public StyledFrame {
	public:
		enum Status {
			Waiting, Transfering, Finished
		};

		TransferWidget ();

	protected:

		//TransferHandler * mTransferHandler;

		// Left info
		IconLabel * mTransferTypeIcon;
		QLabel * mFileDescr;

		// Steps
		BoxWidgetWrapper * mWaitingWidget;
		
		QProgressBar * mTransferingProgressBar;
		
		QLabel * mFinishedStatus;

		QHBoxLayout * mStepsLayout;

		// Right
		IconButton * mCloseAbortButton;

		QHBoxLayout * mMainLayout;

		// Private functions
		void setStatus (Status status);	
};

class InTransferWidget : public TransferWidget {
	public:
		InTransferWidget ();
		
	private:
		// Additionnal widgets
		IconButton * mAcceptButton;
};

class OutTransferWidget : public TransferWidget {
	public:
		OutTransferWidget ();
};

#endif

