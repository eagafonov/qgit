/*
	Description: worktrees list view

	Copyright: See COPYING file that comes with this distribution

*/
#include <QDir>
#include <QKeyEvent>
#include <QMenu>
#include "worktreeview.h"

// a one-column tree, not a list, for the same header as the "Git tree" view
WorktreeView::WorktreeView(QWidget* parent) : QTreeWidget(parent) {

	setContextMenuPolicy(Qt::CustomContextMenu);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setRootIsDecorated(false);
	setColumnCount(1);
	setHeaderLabel("Worktrees");

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

		QString detail;
		if (wt.isBare)
			detail = "bare";
		else if (wt.isDetached || wt.branch.isEmpty())
			detail = "detached: " + wt.head.left(Git::shortHashLenDefault);
		else
			detail = wt.branch;

		QString text(QString("%1 (%2)").arg(QDir(wt.path).dirName(), detail));
		if (wt.isPrunable)
			text += " [prunable]";

		QTreeWidgetItem* item = new QTreeWidgetItem(this, QStringList(text));
		item->setData(0, Qt::UserRole, i);

		QString tip(wt.path);
		if (wt.isLocked)
			tip += "\nlocked" + (wt.lockReason.isEmpty() ? "" : ": " + wt.lockReason);
		if (wt.isPrunable)
			tip += "\nprunable: directory is missing, use 'Prune' to clean up";
		item->setToolTip(0, tip);

		if (wt.isCurrent) {
			QFont f(item->font(0));
			f.setBold(true);
			item->setFont(0, f);
		}
		if (wt.isPrunable) {
			// still selectable, the context menu must work on it
			QFont f(item->font(0));
			f.setItalic(true);
			item->setFont(0, f);
			item->setForeground(0, palette().brush(QPalette::Disabled, QPalette::Text));
		}
		if (wt.path == selectedPath)
			setCurrentItem(item);
	}
}

const Git::WorktreeInfo* WorktreeView::worktreeAt(QTreeWidgetItem* item) const {

	if (!item)
		return NULL;

	bool ok;
	int i = item->data(0, Qt::UserRole).toInt(&ok);
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
