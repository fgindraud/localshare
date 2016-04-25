/*
 * Main window.
 */
#ifndef H_MAINWINDOW
#define H_MAINWINDOW

#include "network.h"
#include "peerWidgets.h"
#include "miscWidgets.h"
#include "common.h"

#include <QtWidgets>

class MainWindow;
class TrayIcon;
class SendFileArea;
class SendFileItem;

/*
 * SendFileArea
 * Used to store files before they can be drag&drop-ed to be sent.
 * Files loaded with "open file" will end up there.
 * Files can be dropped in there to be "stored".
 */
class SendFileArea : public StyledFrame {
	Q_OBJECT
	
	public:
		SendFileArea ();

		// Add a file to the list if the file is valid (exists, readable)
		void addFile (const QString filePath);

	protected:
		// Handle file drop
		void dragEnterEvent (QDragEnterEvent * event);
		void dropEvent (QDropEvent * event);

	private:
		/*
		 * Structure
		 */
		QVBoxLayout * mLayout;
};

/*
 * SendFileItem
 * Store one file. Draggable.
 */
class SendFileItem : public BoxWidgetWrapper {
	Q_OBJECT

	public:
		SendFileItem (const QString file, FileUtils::Size size);

	protected:
		// Draggable stuff
		void mousePressEvent (QMouseEvent * event);
		void mouseMoveEvent (QMouseEvent * event);

	private:
		// Drag & drop threshold holder variable
		QPoint mStartDragPos;

		// Complete filename given to widget we drop this in
		QString mHoldFileName;

		/*
		 * Structure
		 */
		IconLabel * mFileIconLabel;
		QLabel * mFileDescrLabel;
		IconButton * mDelete;
};



#endif

