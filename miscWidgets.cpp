#include "miscWidgets.h"

/* ------ IconButton ------ */
IconButton::IconButton (QIcon icon, const QString tooltip) {
	setIcon (icon);
	setToolTip (tooltip);
}

/* ------ IconLabel ------ */
IconLabel::IconLabel () {}

IconLabel::IconLabel (QIcon icon, const QString tooltip) {
	setupIconLabel (icon, tooltip);
}

void IconLabel::setupIconLabel (QIcon icon, const QString tooltip) {
	// Choose a decent size for icon
	int size = sizeHint ().height ();
	setPixmap (icon.pixmap (size));

	// Alignment settings
	setAlignment (Qt::AlignHCenter | Qt::AlignVCenter);
	setMargin (1);
	
	setToolTip (tooltip);
}

/* ------ BoxWidgetWrapper ------ */
BoxWidgetWrapper::BoxWidgetWrapper (QBoxLayout::Direction dir) {
	// Add layout
	mBoxLayout = new QBoxLayout (dir);
	setLayout (mBoxLayout);
	
	// Set margins to 0
	mBoxLayout->setContentsMargins (QMargins ());
}

/* ------ StyledFrame ------ */
StyledFrame::StyledFrame (int style) {
	setFrameStyle (style | QFrame::StyledPanel);
}

