/* Copyright (C) 2007 - 2011 Jan Kundrát <jkt@flaska.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/


#include "FetchMsgPartTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {

FetchMsgPartTask::FetchMsgPartTask( Model *_model, const QModelIndex &mailbox, const QList<uint> &_uids, const QStringList &_parts ):
    ImapTask( _model ), uids(_uids), parts(_parts), mailboxIndex(mailbox)
{
    Q_ASSERT(!uids.isEmpty());
    conn = model->findTaskResponsibleFor(mailboxIndex);
    conn->addDependentTask(this);
}

void FetchMsgPartTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    Sequence seq = Sequence::fromList(uids);
    tag = parser->uidFetch( seq, parts );
}

bool FetchMsgPartTask::handleFetch( const Imap::Responses::Fetch* const resp )
{
    if (!mailboxIndex.isValid()) {
        _failed("Mailbox disappeared");
        return false;
    }

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ) );
    Q_ASSERT(mailbox);
    model->_genericHandleFetch( mailbox, resp );
    return true;
}

bool FetchMsgPartTask::handleStateHelper( const Imap::Responses::State* const resp )
{
    if ( resp->tag.isEmpty() )
        return false;

    if (!mailboxIndex.isValid()) {
        _failed("Mailbox disappeared");
        return false;
    }

    if ( resp->tag == tag ) {
        if ( resp->kind == Responses::OK ) {
            log("Fetched parts", LOG_MESSAGES);
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ) );
            Q_ASSERT(mailbox);
            QList<TreeItemMessage*> messages = model->findMessagesByUids( mailbox, uids );
            Q_FOREACH( TreeItemMessage *message, messages ) {
                Q_FOREACH( const QString &partId, parts ) {
                    log("Fetched part" + partId, LOG_MESSAGES);
                    model->_finalizeFetchPart( mailbox, message->row() + 1, partId );
                }
            }
            model->changeConnectionState( parser, CONN_STATE_SELECTED );
            _completed();
        } else {
            // FIXME: error handling
            _failed("Part fetch failed");
        }
        return true;
    } else {
        return false;
    }
}

QString FetchMsgPartTask::debugIdentification() const
{
    if (!mailboxIndex.isValid())
        return QString::fromAscii("[invalid mailbox]");

    Q_ASSERT(!uids.isEmpty());
    return QString::fromAscii("%1: parts %2 for UIDs %3")
            .arg(mailboxIndex.data(RoleMailboxName).toString(), parts.join(QLatin1String(", ")),
                 Sequence::fromList(uids).toString());
}

}
}
