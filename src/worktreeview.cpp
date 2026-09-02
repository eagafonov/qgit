/*
	Description: worktrees list view

	Copyright: See COPYING file that comes with this distribution

*/
#include <QDir>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include "worktreeview.h"

enum WorktreeColumn { NAME_COL, BRANCH_COL, PATH_COL, COL_COUNT };

// a flat multi-column tree, a list would have no header
WorktreeView::WorktreeView(QWidget* parent) : QTreeWidget(parent) {

	setContextMenuPolicy(Qt::CustomContextMenu);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setRootIsDecorated(false);
	setAllColumnsShowFocus(true);
	setColumnCount(COL_COUNT);
	setHeaderLabels(QStringList() << "Worktree" << "Branch" << "Path");
	// no sorting: row 0 must stay the main worktree, see canRemove()
	setSortingEnabled(false);
	header()->setSectionsMovable(false);
	header()->setStretchLastSection(true);

	// not itemActivated(): it fires on single click when the style says so
	// (e.g. KDE single-click setting), and switching reloads the repository
	connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
	        this, SLOT(on_itemActivated(QTreeWidgetItem*)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
	        this, SLOT(on_customContextMenuRequested(const QPoint&)));
}

void WorktreeView::setWorktrees(const Git::WorktreeList& list) {

	QTreeWidgetItem* cur = currentItem();
	const Git::WorktreeInfo* curWt = cur ? worktreeAt(cur) : NULL;
	const QString selectedPath(curWt ? curWt->path : QString());

	clear();
	worktrees = list;

	for (int i = 0; i < worktrees.size(); i++) {
		const Git::WorktreeInfo& wt = worktrees.at(i);

		QString branch;
		if (wt.isBare)
			branch = "(bare)";
		else if (wt.isDetached || wt.branch.isEmpty())
			branch = "(detached: " + wt.head.left(Git::shortHashLenDefault) + ")";
		else
			branch = wt.branch;

		QString name(QDir(wt.path).dirName());
		if (wt.isPrunable)
			name += " [prunable]";
		else if (wt.isLocked)
			name += " [locked]";

		QStringList cols;
		cols << name << branch << QDir::toNativeSeparators(wt.path);
		QTreeWidgetItem* item = new QTreeWidgetItem(this, cols);
		item->setData(NAME_COL, Qt::UserRole, i);

		QString tip(wt.path);
		if (wt.isLocked)
			tip += "\nlocked" + (wt.lockReason.isEmpty() ? "" : ": " + wt.lockReason);
		if (wt.isPrunable)
			tip += "\nprunable: directory is missing, use 'Prune' to clean up";

		for (int c = 0; c < COL_COUNT; c++) {
			item->setToolTip(c, tip);
			QFont f(item->font(c));
			f.setBold(wt.isCurrent);
			// still selectable, the context menu must work on it
			f.setItalic(wt.isPrunable);
			item->setFont(c, f);
			if (wt.isPrunable)
				item->setForeground(c, palette().brush(QPalette::Disabled, QPalette::Text));
		}
		if (wt.path == selectedPath)
			setCurrentItem(item);
	}
	// the last column stretches to fill the remaining width
	for (int c = 0; c < COL_COUNT - 1; c++)
		resizeColumnToContents(c);
}

const Git::WorktreeInfo* WorktreeView::worktreeAt(QTreeWidgetItem* item) const {

	if (!item)
		return NULL;

	bool ok;
	int i = item->data(NAME_COL, Qt::UserRole).toInt(&ok);
	return (ok && i >= 0 && i < worktrees.size()) ? &worktrees.at(i) : NULL;
}

const Git::WorktreeInfo* WorktreeView::worktreeOf(SCRef path) const {

	for (int i = 0; i < worktrees.size(); i++)
		if (worktrees.at(i).path == path)
			return &worktrees.at(i);
	return NULL;
}

bool WorktreeView::canSwitch(const Git::WorktreeInfo& wt) {

	return !wt.isCurrent && !wt.isBare && !wt.isPrunable;
}

bool WorktreeView::canRemove(const Git::WorktreeInfo& wt, bool isMain) {

	// git refuses to remove the main worktree, and qgit must not delete the
	// directory it is showing
	// a prunable worktree has no directory to remove, it is pruned instead
	return !isMain && !wt.isCurrent && !wt.isBare && !wt.isPrunable;
}

bool WorktreeView::hasPrunable() const {

	for (int i = 0; i < worktrees.size(); i++)
		if (worktrees.at(i).isPrunable)
			return true;
	return false;
}

void WorktreeView::keyPressEvent(QKeyEvent* e) {

	if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
		on_itemActivated(currentItem());
		return;
	}
	QTreeWidget::keyPressEvent(e);
}

void WorktreeView::on_itemActivated(QTreeWidgetItem* item) {

	const Git::WorktreeInfo* wt = worktreeAt(item);
	if (!wt || wt->isCurrent)
		return;

	if (canSwitch(*wt))
		emit switchRequested(wt->path);
	else if (wt->isPrunable)
		emit showStatusMessage("Cannot switch: worktree directory " + wt->path
		                       + " is missing, use 'Prune' to clean up", 5000);
	else if (wt->isBare)
		emit showStatusMessage("Cannot switch to a bare repository entry", 5000);
}

void WorktreeView::on_customContextMenuRequested(const QPoint& pos) {

	QTreeWidgetItem* item = itemAt(pos);
	const Git::WorktreeInfo* wt = worktreeAt(item);

	QMenu menu(this);
	QAction* actSwitch = menu.addAction("&Switch to worktree");
	QAction* actRemove = menu.addAction("&Remove worktree...");
	QAction* actPrune = NULL;
	if (hasPrunable())
		actPrune = menu.addAction("&Prune missing worktrees...");
	menu.addSeparator();
	QAction* actRefresh = menu.addAction("Re&fresh");

	bool isMain = (item && indexOfTopLevelItem(item) == 0);
	actSwitch->setEnabled(wt && canSwitch(*wt));
	actRemove->setEnabled(wt && canRemove(*wt, isMain));

	// copy the path: the triggered action may cause a list update
	const QString path(wt ? wt->path : QString());

	QAction* act = menu.exec(viewport()->mapToGlobal(pos));
	if (act == actSwitch)
		emit switchRequested(path);
	else if (act == actRemove)
		emit removeRequested(path);
	else if (act && act == actPrune)
		emit pruneRequested();
	else if (act == actRefresh)
		emit refreshRequested();
}
