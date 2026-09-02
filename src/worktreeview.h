/*
	Description: worktrees list view

	Copyright: See COPYING file that comes with this distribution

*/
#ifndef WORKTREEVIEW_H
#define WORKTREEVIEW_H

#include <QTreeWidget>
#include "git.h"

class WorktreeView : public QTreeWidget {
Q_OBJECT
public:
	explicit WorktreeView(QWidget* parent);
	void setWorktrees(const Git::WorktreeList& list);
	const Git::WorktreeInfo* worktreeOf(SCRef path) const;

signals:
	void switchRequested(const QString& path);
	void removeRequested(const QString& path);
	void refreshRequested();
	void pruneRequested();
	void showStatusMessage(const QString& msg, int timeout);

protected:
	virtual void keyPressEvent(QKeyEvent* e);

private slots:
	void on_itemActivated(QTreeWidgetItem*);
	void on_customContextMenuRequested(const QPoint&);

private:
	const Git::WorktreeInfo* worktreeAt(QTreeWidgetItem* item) const;
	static bool canSwitch(const Git::WorktreeInfo& wt);
	static bool canRemove(const Git::WorktreeInfo& wt, bool isMain);
	bool hasPrunable() const;

	Git::WorktreeList worktrees;
};

#endif
