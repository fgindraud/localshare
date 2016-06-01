#pragma once
#ifndef CLI_INDICATOR_H
#define CLI_INDICATOR_H

#include <QString>
#include <vector>

#include "core_localshare.h"

namespace Cli {
namespace Indicator {
	/* Draw progress indicator.
	 * Small QWidget inspired system to resize to terminal width.
	 * Items are stored by reference, and there is no ownership system, so beware !
	 */
	struct Item {
		virtual int min_size (void) const { return 0; }        // minimum size required for display
		virtual bool expandable (void) const { return false; } // indicates if the item will fit size
		virtual QString draw (int len) const = 0;              // len >= min_size();
	};

	// Container
	class Container : public Item {
		// Weight is used to balance unused space for expandable elements
		// If len is smaller than min_size (for top level), elements will be excluded by priority
		// Cache expandable() calls, so do not modify it in sub elements
	public:
		struct Element {
			Element (const Item & item, int priority, qreal weight)
			    : item (item), priority (priority), weight (weight) {}
			const Item & item;
			int priority;
			qreal weight;
		};

	private:
		std::vector<Element> items;
		bool can_expand{false};
		const QString sep;

	public:
		Container (const QString & sep = QString ()) : sep (sep) {}
		Container & append (const Item & item, int priority = 0, qreal weight = 0) {
			Q_ASSERT (weight >= 0);
			items.emplace_back (item, priority, weight);
			can_expand = can_expand || item.expandable ();
			return *this;
		}
		Element & settings_of (const Item & item) {
			for (auto & e : items)
				if (&(e.item) == &item)
					return e;
			Q_UNREACHABLE ();
		}
		bool expandable (void) const Q_DECL_OVERRIDE { return can_expand; }
		int min_size (void) const Q_DECL_OVERRIDE {
			int s = sep.size () * qMax (0, int(items.size ()) - 1);
			for (auto & e : items)
				s += e.item.min_size ();
			return s;
		}
		QString draw (int len) const Q_DECL_OVERRIDE {
			int nb_shown = items.size ();
			std::vector<bool> shown (nb_shown, true);
			int size_required = min_size ();
			// Hide elements by priority if not enough space
			while (size_required > len && nb_shown > 0) {
				// Hide smallest priority
				int min_priority = items[0].priority;
				int index = 0;
				for (std::size_t i = 1; i < items.size (); ++i) {
					if (shown[i] && items[i].priority < min_priority) {
						min_priority = items[i].priority;
						index = i;
					}
				}
				shown[index] = false;
				size_required -= items[index].item.min_size ();
				if (nb_shown > 1)
					size_required -= sep.size ();
				nb_shown--;
			}
			// Determine size of elements and draw
			QString buf;
			qreal total_weight = 0;
			qreal default_ratio = 0; // used by all when total_weight == 0 (no weight set)
			if (can_expand) {
				buf.reserve (len);
				for (std::size_t i = 0; i < items.size (); ++i) {
					if (shown[i] && items[i].item.expandable ()) {
						total_weight += items[i].weight;
						default_ratio += 1;
					}
				}
				// If default ratio is 0, then all expandable items are hidden
				if (default_ratio > 0)
					default_ratio = 1 / default_ratio;
			} else {
				buf.reserve (size_required);
			}
			int unused_space_total = len - size_required;
			int unused_space_left = unused_space_total;
			for (std::size_t i = 0; i < items.size (); ++i) {
				if (shown[i]) {
					auto & e = items[i];
					int s = e.item.min_size ();
					if (e.item.expandable ()) {
						auto unused_space_share = total_weight > 0 ? e.weight / total_weight : default_ratio;
						int additional_space =
						    qMin (unused_space_left, qRound (unused_space_total * unused_space_share));
						s += additional_space;
						unused_space_left -= additional_space;
					}
					if (i > 0)
						buf += sep;
					buf += e.item.draw (s);
				}
			}
			return buf;
		}
	};

	// Basic elements
	struct FixedChar : public Item {
		const QChar c;
		FixedChar (QChar c) : c (c) {}
		int min_size (void) const Q_DECL_OVERRIDE { return 1; }
		QString draw (int) const Q_DECL_OVERRIDE { return c; }
	};
	struct RepeatedChar : public Item {
		const QChar c;
		RepeatedChar (QChar c) : c (c) {}
		bool expandable (void) const Q_DECL_OVERRIDE { return true; }
		QString draw (int len) const Q_DECL_OVERRIDE { return QString (len, c); }
	};
	struct FixedString : public Item {
		const QString str;
		FixedString (const QString & str) : str (str) {}
		int min_size (void) const Q_DECL_OVERRIDE { return str.size (); }
		QString draw (int) const Q_DECL_OVERRIDE { return str; }
	};
	struct Percent : public Item {
		qreal value;
		Percent (qreal value = 0) : value (value) {}
		int min_size (void) const Q_DECL_OVERRIDE { return 4; }
		QString draw (int) const Q_DECL_OVERRIDE {
			return QStringLiteral ("%1%").arg (int(100 * value), 3);
		}
	};
	struct ProgressNumber : public Item {
		int current;
		const int max;
		const int num_width;
		ProgressNumber (int max, int current = 0)
		    : current (current), max (max), num_width (QString::number (max).size ()) {}
		int min_size (void) const Q_DECL_OVERRIDE { return 1 + 2 * num_width; }
		QString draw (int) const Q_DECL_OVERRIDE {
			return QStringLiteral ("%1/%2").arg (current, num_width).arg (max);
		}
	};
	struct ByteRate : public Item {
		qint64 current;
		const int size_width;
		ByteRate (qint64 current = 0)
		    : current (current), size_width (size_to_string (1023 * 1024).size ()) {}
		int min_size (void) const Q_DECL_OVERRIDE {
			return qMax (size_width, size_to_string (current).size ()) + 2;
		}
		QString draw (int len) const Q_DECL_OVERRIDE {
			return QStringLiteral ("%1/s").arg (size_to_string (current), len - 2);
		}
	};

	// Compound elements
	class ProgressBar : public Container {
	private:
		const FixedChar opening{'['};
		const RepeatedChar filled{'#'};
		const RepeatedChar empty{'-'};
		const FixedChar ending{']'};
		using Container::append;
		using Container::settings_of;

	public:
		ProgressBar (qreal ratio = 0) {
			append (opening);
			append (filled, 0, ratio);
			append (empty, 0, 1 - ratio);
			append (ending);
		}
		void set_ratio (qreal ratio) {
			settings_of (filled).weight = ratio;
			settings_of (empty).weight = 1 - ratio;
		}
	};
}
}

#endif
