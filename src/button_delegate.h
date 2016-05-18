#ifndef BUTTON_DELEGATE_H
#define BUTTON_DELEGATE_H

#include <QList>
#include <QAbstractItemDelegate>
#include <QApplication>
#include <QStyle>
#include <QStyleOptionButton>
#include <QMouseEvent>
#include <QPersistentModelIndex>

class ButtonDelegate : public QAbstractItemDelegate {
	Q_OBJECT

	/* Add support for ButtonRole by appending buttons to cells.
	 * Must have an "inner_delegate" that handles the content outside of buttons.
	 *
	 * Buttons are placed on the right of each cell.
	 * Their order is fixed and determined by the supported_buttons list order.
	 * When drawn, buttons only take their sizeHint, and are vertically centered.
	 *
	 * sizeHint: their required size is added to inner_sizeHint to the right.
	 * paint: paint buttons from right to left, shaving space of the item.
	 * event: "paint" to get positions, then check rects.
	 *
	 */
public:
	enum Role { ButtonRole = Qt::UserRole }; // Custom role for buttons

private:
	class HoverTarget {
		// Helper class to manage tracking of hovered button
	private:
		QPersistentModelIndex index{QModelIndex ()};
		int button{0};
		bool mouse_pressed{false};

	public:
		bool is_valid (void) const { return index.isValid () && button != 0; }

		void reset (QAbstractItemModel * model) {
			if (is_valid ()) {
				repaint_button (model, index);
				*this = HoverTarget ();
			}
		}
		void set (QAbstractItemModel * model, const QModelIndex & new_index, int new_button,
		          bool mouse_state) {
			if (!is_valid () || index != new_index || button != new_button ||
			    mouse_pressed != mouse_state) {
				index = new_index;
				button = new_button;
				mouse_pressed = mouse_state;
				repaint_button (model, index);
			}
		}

		bool is_hovering (const QModelIndex & test_index, int test_button) const {
			return is_valid () && index == test_index && button == test_button;
		}

		bool is_mouse_pressed (void) const { return mouse_pressed; }

	private:
		void repaint_button (QAbstractItemModel * model, const QModelIndex & index) const {
			// For now, send signal dataChanged in model to force repainting
			QMetaObject::invokeMethod (model, "dataChanged", Q_ARG (QModelIndex, index),
			                           Q_ARG (QModelIndex, index), Q_ARG (QVector<int>, {ButtonRole}));
		}
	};

protected:
	struct SupportedButton {
		int flag;
		QIcon icon;
	};
	QList<SupportedButton> supported_buttons; // Should be filled by subclasses

	void set_inner_delegate (QAbstractItemDelegate * delegate) {
		Q_ASSERT (inner_delegate == nullptr); // Only set once
		inner_delegate = delegate;
		// Propagate signals
		connect (delegate, &QAbstractItemDelegate::closeEditor, this,
		         &QAbstractItemDelegate::closeEditor);
		connect (delegate, &QAbstractItemDelegate::commitData, this,
		         &QAbstractItemDelegate::commitData);
		connect (delegate, &QAbstractItemDelegate::sizeHintChanged, this,
		         &QAbstractItemDelegate::sizeHintChanged);
	}

private:
	HoverTarget hover_target; // Current hovered button (invalid if no button hovered)
	QAbstractItemDelegate * inner_delegate{nullptr}; // Handles cell content (does not take ownership)

signals:
	void button_clicked (QModelIndex index, int btn);

public:
	ButtonDelegate (QObject * parent = nullptr) : QAbstractItemDelegate (parent) {}

	QWidget * createEditor (QWidget * parent, const QStyleOptionViewItem & option,
	                        const QModelIndex & index) const Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		return inner_delegate->createEditor (parent, option, index);
	}

	void destroyEditor (QWidget * editor, const QModelIndex & index) const Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		inner_delegate->destroyEditor (editor, index);
	}

	bool editorEvent (QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option,
	                  const QModelIndex & index) Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		// If no buttons redirect event to QStyledItemDelegate
		auto buttons = get_button_flags (index);
		if (buttons == 0) {
			if (event->type () == QEvent::MouseMove)
				hover_target.reset (model); // Mouse is outside a button
			return inner_delegate->editorEvent (event, model, option, index);
		}

		/* If buttons, determine button hitboxes to see who handles events.
		 * Note that our buttons never get keyboard focus, so they only catch mouse events.
		 * Unless the event (mouse event) has been handled by a button, we continue.
		 * This is used to determine content hitbox for QStyledItemDelegate::editorEvent.
		 */
		QStyleOptionViewItem inner_option (option);
		for (auto sb = supported_buttons.rbegin (); sb != supported_buttons.rend (); ++sb) {
			if (test_flag (buttons, sb->flag)) {
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
				init_button_style (opt, inner_option, index, *sb);
				auto button_size = get_button_size (opt);
				opt.rect = carve_button_rect (inner_option.rect, button_size);

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
		return inner_delegate->editorEvent (event, model, inner_option, index);
	}

	bool eventFilter (QObject * editor, QEvent * event) Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		return inner_delegate->eventFilter (editor, event);
	}

	bool helpEvent (QHelpEvent * event, QAbstractItemView * view, const QStyleOptionViewItem & option,
	                const QModelIndex & index) Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		return inner_delegate->helpEvent (event, view, option, index);
	}

	void paint (QPainter * painter, const QStyleOptionViewItem & option,
	            const QModelIndex & index) const Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		// If no button paint content directly
		auto buttons = get_button_flags (index);
		if (buttons == 0) {
			inner_delegate->paint (painter, option, index);
			return;
		}

		// Paint buttons then paint content in shaved rect
		QStyleOptionViewItem inner_option (option);
		for (auto sb = supported_buttons.rbegin (); sb != supported_buttons.rend (); ++sb) {
			if (test_flag (buttons, sb->flag)) {
				// Draw button
				QStyleOptionButton opt;
				init_button_style (opt, inner_option, index, *sb);
				auto button_size = get_button_size (opt);
				opt.rect = carve_button_rect (inner_option.rect, button_size);
				QApplication::style ()->drawControl (QStyle::CE_PushButton, &opt, painter);
			}
		}
		inner_delegate->paint (painter, inner_option, index);
	}

	void setEditorData (QWidget * editor, const QModelIndex & index) const Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		inner_delegate->setEditorData (editor, index);
	}

	void setModelData (QWidget * editor, QAbstractItemModel * model,
	                   const QModelIndex & index) const {
		Q_ASSERT (inner_delegate);
		inner_delegate->setModelData (editor, model, index);
	}

	QSize sizeHint (const QStyleOptionViewItem & option,
	                const QModelIndex & index) const Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		// Get content sizeHint, return it directly if no buttons
		auto inner_size = inner_delegate->sizeHint (option, index);
		auto buttons = get_button_flags (index);
		if (buttons == 0)
			return inner_size;

		// Add sizeHints from buttons, packed to the right
		for (auto & sb : supported_buttons) {
			if (test_flag (buttons, sb.flag)) {
				// Add the button to the right
				QStyleOptionButton opt;
				init_button_style (opt, option, index, sb);
				auto button_size = get_button_size (opt);
				inner_size = inner_size.expandedTo (
				    {inner_size.width () + button_size.width (), button_size.height ()});
			}
		}
		return inner_size;
	}

	void updateEditorGeometry (QWidget * editor, const QStyleOptionViewItem & option,
	                           const QModelIndex & index) const Q_DECL_OVERRIDE {
		Q_ASSERT (inner_delegate);
		inner_delegate->updateEditorGeometry (editor, option, index);
	}

private:
	int get_button_flags (const QModelIndex & index) const {
		if (index.isValid ()) {
			auto v = index.data (ButtonRole);
			if (v.canConvert<int> ())
				return v.toInt ();
		}
		return 0;
	}

	bool test_flag (int buttons, int button) const { return (buttons & button) != 0; }

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
};

#endif
