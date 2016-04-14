/*
 * Miscellaneous widgets.
 */
#ifndef H_MISCWIDGETS
#define H_MISCWIDGETS

#include <QtWidgets>

/*
 * Button with icon and tooltip.
 */
class IconButton : public QPushButton {
	public:
		IconButton (QIcon icon, const QString tooltip = QString ());
};

/*
 * Label with icon and tooltip
 */
class IconLabel : public QLabel {
	public:
		// Immediate setting
		IconLabel (QIcon icon, const QString tooltip = QString ());
	
		// Delayed setting
		IconLabel ();
		void setupIconLabel (QIcon icon, const QString tooltip = QString ());
};

/*
 * QWidget wrapper around a BoxLayout, with no additional space taken
 */
class BoxWidgetWrapper : public QWidget {
	public:
		// Construct the BoxLayout, and set style to remove spacing
		BoxWidgetWrapper (QBoxLayout::Direction dir);

		// Need to add a direct reference to BoxLayout (layout method returns a
		// QLayout, which is not sufficient)
		QBoxLayout * mBoxLayout;
};

/*
 * QFrame with simplified constructor
 */
class StyledFrame : public QFrame {
	public:
		StyledFrame (int style = QFrame::Raised);
};

#endif

