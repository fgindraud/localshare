#include "transfer.h"


/* ------ Transfer widget ------ */

TransferWidget::TransferWidget () : StyledFrame () {
	// Left
	mTransferTypeIcon = new IconLabel;

	mFileDescr = new QLabel; 

	// Steps

	mWaitingWidget = new BoxWidgetWrapper (QBoxLayout::LeftToRight);
	mWaitingWidget->mBoxLayout->addStretch (1);
	
	mTransferingProgressBar = new QProgressBar;

	mFinishedStatus = new QLabel;

	mStepsLayout = new QHBoxLayout;
	mStepsLayout->addWidget (mWaitingWidget);
	mStepsLayout->addWidget (mTransferingProgressBar);
	mStepsLayout->addWidget (mFinishedStatus);

	// Button
	mCloseAbortButton = new IconButton (Icon::closeAbort (), "Close connection");

	// Main layout
	mMainLayout = new QHBoxLayout;
	mMainLayout->addWidget (mTransferTypeIcon);
	mMainLayout->addWidget (mFileDescr);
	mMainLayout->addLayout (mStepsLayout, 1);
	mMainLayout->addWidget (mCloseAbortButton);

	// Frame
	setLayout (mMainLayout);

	// Start with waiting status
	setStatus (Waiting);

	// Connections

}

void TransferWidget::setStatus (TransferWidget::Status status) {
	mWaitingWidget->setVisible (status == Waiting);
	mTransferingProgressBar->setVisible (status == Transfering);
	mFinishedStatus->setVisible (status == Finished);
}

InTransferWidget::InTransferWidget () : TransferWidget () {
	mTransferTypeIcon->setupIconLabel (Icon::inbound (), "Download");

	// Add accept button to waiting widget
	mAcceptButton = new IconButton (Icon::accept (), "Accept connection");
	mWaitingWidget->mBoxLayout->addWidget (mAcceptButton);
}

OutTransferWidget::OutTransferWidget () : TransferWidget () {
	mTransferTypeIcon->setupIconLabel (Icon::outbound (), "Upload");
}
