#ifndef TRANSFER_DELEGATE_H
#define TRANSFER_DELEGATE_H

#include <QList>
#include <QApplication>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionProgressBar>
#include <QStyleOptionButton>
#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <array>

#include "style.h"
#include "transfer_model.h"

namespace Transfer {

class Delegate : public QStyledItemDelegate {
	Q_OBJECT

	/* Add support for ButtonRole by appending buttons to cells.
	 * Also adds a progress bar for ProgressField.
	 *
	 * Buttons are placed on the right of each cell.
	 * Their order is fixed and determined by the supported_buttons list order.
	 * When drawn, buttons only take their sizeHint, and are vertically centered.
	 *
	 * sizeHint: their required size is added to content_sizeHint to the right.
	 * paint: paint buttons from right to left, shaving space of the item.
	 * event: "paint" to get positions, then check rects.
	 */
private:
	class HoverTarget {
		// Helper class to manage tracking of hovered button
	private:
		QPersistentModelIndex index{QModelIndex ()};
		Item::Button button{Item::NoButton};
		bool mouse_pressed{false};

	public:
		bool is_valid (void) const { return index.isValid () && button != Item::NoButton; }

		void reset (QAbstractItemModel * model) {
			if (is_valid ()) {
				repaint_button (model, index);
				*this = HoverTarget ();
			}
		}
		void set (QAbstractItemModel * model, const QModelIndex & new_index, Item::Button new_button,
		          bool mouse_state) {
			if (!is_valid () || index != new_index || button != new_button ||
			    mouse_pressed != mouse_state) {
				index = new_index;
				button = new_button;
				mouse_pressed = mouse_state;
				repaint_button (model, index);
			}
		}

		bool is_hovering (const QModelIndex & test_index, Item::Button test_button) const {
			return is_valid () && index == test_index && button == test_button;
		}

		bool is_mouse_pressed (void) const { return mouse_pressed; }

	private:
		void repaint_button (QAbstractItemModel * model, const QModelIndex & index) const {
			// For now, send signal dataChanged in model to force repainting
			QMetaObject::invokeMethod (model, "dataChanged", Q_ARG (QModelIndex, index),
			                           Q_ARG (QModelIndex, index),
			                           Q_ARG (QVector<int>, {Item::ButtonRole}));
		}
	};

private:
	struct SupportedButton {
		Item::Button flag;
		QIcon icon;
	};
	QList<SupportedButton> supported_buttons;

	HoverTarget hover_target; // Current hovered button (invalid if no button hovered)

signals:
	void button_clicked (QModelIndex index, Item::Button btn);

public:
	Delegate (QObject * parent = nullptr) : QStyledItemDelegate (parent) {
		supported_buttons << SupportedButton{Item::AcceptButton, Icon::accept ()}
		                  << SupportedButton{Item::CancelButton, Icon::cancel ()}
		                  << SupportedButton{Item::ChangeDownloadPathButton,
		                                     Icon::change_download_path ()}
		                  << SupportedButton{Item::DeleteButton, Icon::delete_transfer ()};
	}

	void paint (QPainter * painter, const QStyleOptionViewItem & option,
	            const QModelIndex & index) const Q_DECL_OVERRIDE {
		// If no button paint content directly
		auto buttons = get_button_flags (index);
		if (buttons == Item::NoButton) {
			content_paint (painter, option, index);
			return;
		}

		// Paint buttons then paint content in shaved rect
		QStyleOptionViewItem content_option (option);
		for (auto sb = supported_buttons.rbegin (); sb != supported_buttons.rend (); ++sb) {
			if (buttons.testFlag (sb->flag)) {
				// Draw button
				QStyleOptionButton opt;
				init_button_style (opt, content_option, index, *sb);
				auto button_size = get_button_size (opt);
				opt.rect = carve_button_rect (content_option.rect, button_size);
				QApplication::style ()->drawControl (QStyle::CE_PushButton, &opt, painter);
			}
		}
		content_paint (painter, content_option, index);
	}

	QSize sizeHint (const QStyleOptionViewItem & option,
	                const QModelIndex & index) const Q_DECL_OVERRIDE {
		// Get content sizeHint, return it directly if no buttons
		auto content_size = QStyledItemDelegate::sizeHint (option, index);
		auto buttons = get_button_flags (index);
		if (buttons == Item::NoButton)
			return content_size;

		// Add sizeHints from buttons, packed to the right
		for (auto & sb : supported_buttons) {
			if (buttons.testFlag (sb.flag)) {
				// Add the button to the right
				QStyleOptionButton opt;
				init_button_style (opt, option, index, sb);
				auto button_size = get_button_size (opt);
				content_size = content_size.expandedTo (
				    {content_size.width () + button_size.width (), button_size.height ()});
			}
		}
		return content_size;
	}

	bool editorEvent (QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option,
	                  const QModelIndex & index) Q_DECL_OVERRIDE {
		// If no buttons redirect event to QStyledItemDelegate
		auto buttons = get_button_flags (index);
		if (buttons == Item::NoButton) {
			if (event->type () == QEvent::MouseMove)
				hover_target.reset (model); // Mouse is outside a button
			return QStyledItemDelegate::editorEvent (event, model, option, index);
		}

		/* If buttons, determine button hitboxes to see who handles events.
		 * Note that our buttons never get keyboard focus, so they only catch mouse events.
		 * Unless the event (mouse event) has been handled by a button, we continue.
		 * This is used to determine content hitbox for QStyledItemDelegate::editorEvent.
		 */
		QStyleOptionViewItem content_option (option);
		for (auto sb = supported_buttons.rbegin (); sb != supported_buttons.rend (); ++sb) {
			if (buttons.testFlag (sb->flag)) {
				// Check if current button handles the event
				switch (event->type ()) {
				case QEvent::MouseButtonPress:
				case QEvent::MouseButtonRelease:
				case QEvent::MouseMove:
					break;
				default:
					// Event not from the mouse are left for QStyledItemDelegate
					continue;
				}
				// Place button
				QStyleOptionButton opt;
				init_button_style (opt, content_option, index, *sb);
				auto button_size = get_button_size (opt);
				opt.rect = carve_button_rect (content_option.rect, button_size);

				// Test where the cursor is
				auto mev = static_cast<QMouseEvent *> (event);
				if (!opt.rect.contains (mev->pos ()))
					continue; // Not in our rect
				auto hit_button = QApplication::style ()
				                      ->subElementRect (QStyle::SE_PushButtonFocusRect, &opt)
				                      .contains (mev->pos ());
				if (!hit_button) {
					// Not in button hitbox
					hover_target.reset (model);
				} else {
					// In button hitbox
					hover_target.set (model, index, sb->flag, mev->buttons ().testFlag (Qt::LeftButton));
					if (event->type () == QEvent::MouseButtonRelease && mev->button () == Qt::LeftButton) {
						emit button_clicked (index, sb->flag);
					}
				}
				return event->type () != QEvent::MouseMove; // Let MouseMove propagate
			}
		}
		return QStyledItemDelegate::editorEvent (event, model, content_option, index);
	}

private:
	Item::Buttons get_button_flags (const QModelIndex & index) const {
		if (index.isValid ()) {
			auto v = index.data (Item::ButtonRole);
			if (v.canConvert<Item::Buttons> ())
				return v.value<Item::Buttons> ();
		}
		return Item::NoButton;
	}

	// Button sizing

	QSize get_button_size (const QStyleOptionButton & option) const {
		auto button_icon_size = QApplication::style ()->pixelMetric (QStyle::PM_ButtonIconSize);
		return QApplication::style ()
		    ->sizeFromContents (QStyle::CT_PushButton, &option,
		                        QSize (button_icon_size, button_icon_size))
		    .expandedTo (QApplication::globalStrut ());
	}

	QRect carve_button_rect (QRect & rect, const QSize & button_size) const {
		if (rect.width () < button_size.width () || rect.height () < button_size.height ())
			return {};                                                            // Too small to fit
		rect.setWidth (rect.width () - button_size.width ());                   // Carve rect
		return QRect (rect.x () + rect.width (),                                // On the right
		              rect.y () + (rect.height () - button_size.height ()) / 2, // Vertical center
		              button_size.width (), button_size.height ());             // Sizehint size
	}

	// StyleOption init

	void init_button_style (QStyleOptionButton & option, const QStyleOption & from,
	                        const QModelIndex & index, const SupportedButton & button) const {
		option.QStyleOption::operator=(from); // Take palette, ...
		option.rect = QRect ();               // But NOT rect, which will be computed separately
		option.icon = button.icon;
		auto button_icon_size = QApplication::style ()->pixelMetric (QStyle::PM_ButtonIconSize);
		option.iconSize = QSize (button_icon_size, button_icon_size);
		option.state = QStyle::State_Enabled;
		if (hover_target.is_hovering (index, button.flag)) {
			option.state |= QStyle::State_MouseOver;
			if (hover_target.is_mouse_pressed ()) {
				option.state |= QStyle::State_Sunken;
			} else {
				option.state |= QStyle::State_Raised;
			}
		} else {
			option.state |= QStyle::State_Raised;
		}
	}

	void init_progress_bar_style (QStyleOptionProgressBar & option, const QStyleOption & from,
	                              const QModelIndex & index) const {
		option.QStyleOption::operator=(from); // Take palette, ..., AND rect
		option.minimum = 0;
		option.maximum = 100;
		option.progress = -1;
		auto value = index.data (Qt::DisplayRole);
		if (value.canConvert<int> ()) {
			auto v = value.toInt ();
			option.progress = v;
			option.text = QString ("%1%").arg (v);
			option.textVisible = true;
		}
	}

	// Content management

	void content_paint (QPainter * painter, const QStyleOptionViewItem & option,
	                    const QModelIndex & index) const {
		// Paint content of cell
		switch (index.column ()) {
		case Item::ProgressField: {
			// Draw a progress bar
			QStyleOptionProgressBar opt;
			init_progress_bar_style (opt, option, index);
			QApplication::style ()->drawControl (QStyle::CE_ProgressBar, &opt, painter);
		} break;
		default:
			QStyledItemDelegate::paint (painter, option, index);
		}
	}

	QSize content_sizehint (const QStyleOptionViewItem & option, const QModelIndex & index) const {
		switch (index.column ()) {
		case Item::ProgressField: {
			QStyleOptionProgressBar opt;
			init_progress_bar_style (opt, option, index);
			return QApplication::style ()->sizeFromContents (QStyle::CT_ProgressBar, &opt, QSize ());
		} break;
		default:
			return QStyledItemDelegate::sizeHint (option, index);
		}
	}
};
}

#endif