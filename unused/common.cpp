namespace Message {
void error (const QString & title, const QString & message) {
	QMessageBox::critical (0, title, message);
	qApp->quit ();
}

void warning (const QString & title, const QString & message) {
	QMessageBox::warning (0, title, message);
}
}

/* ------ File size ------ */

namespace FileUtils {
QString sizeToString (Size size) {
	double num = size;
	double increment = 1024.0;
	const char * suffixes[] = {"o", "kio", "Mio", "Gio", "Tio", nullptr};
	int unit_idx = 0;
	while (num >= increment && suffixes[unit_idx + 1] != nullptr) {
		unit_idx++;
		num /= increment;
	}
	return QString ().setNum (num, 'f', 2) + suffixes[unit_idx];
}

bool infoCheck (const QString filename, Size * size, QString * baseName) {
	QFileInfo fileInfo (filename);
	if (fileInfo.exists () && fileInfo.isFile () && fileInfo.isReadable ()) {
		if (size != 0)
			*size = fileInfo.size ();
		if (baseName != 0)
			*baseName = fileInfo.fileName ();
		return true;
	} else {
		return false;
	}
}
}
