/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

#include <QColor>
#include <QFont>
#include <QLocale>

#include "vogleditor_qapicalltreemodel.h"

#include "vogl_trace_file_reader.h"
#include "vogl_trace_packet.h"
#include "vogl_trace_stream_types.h"
#include "vogleditor_gl_state_snapshot.h"
#include "vogleditor_apicalltreeitem.h"
#include "vogleditor_frameitem.h"
#include "vogleditor_groupitem.h"
#include "vogleditor_apicallitem.h"
#include "vogleditor_output.h"

vogleditor_QApiCallTreeModel::vogleditor_QApiCallTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    m_rootItem = vogl_new(vogleditor_apiCallTreeItem, this);
}

vogleditor_QApiCallTreeModel::~vogleditor_QApiCallTreeModel()
{
    if (m_rootItem != NULL)
    {
        vogl_delete(m_rootItem);
        m_rootItem = NULL;
    }

    if (m_pTrace_ctypes != NULL)
    {
        vogl_delete(m_pTrace_ctypes);
        m_pTrace_ctypes = NULL;
    }

    m_itemList.clear();
}

bool vogleditor_QApiCallTreeModel::init(vogl_trace_file_reader* pTrace_reader)
{
    const vogl_trace_stream_start_of_file_packet &sof_packet = pTrace_reader->get_sof_packet();
    VOGL_NOTE_UNUSED(sof_packet);

    uint64_t total_swaps = 0;
    bool found_eof_packet = false;

    // make a PendingFrame node to hold the api calls
    // this will remain in the pending state until the first
    // api call is seen, then it will be made the CurFrame and
    // appended to the parent
    vogleditor_frameItem *pCurFrame = NULL;
    vogleditor_groupItem *pCurGroup = NULL;
    vogleditor_apiCallTreeItem* pParentRoot = m_rootItem;
    vogleditor_apiCallTreeItem* pCurParent = pParentRoot;

    // Make a PendingSnapshot that may or may not be populated when reading the trace.
    // This snapshot will be assigned to the next API call that occurs.
    vogleditor_gl_state_snapshot* pPendingSnapshot = NULL;

    m_pTrace_ctypes = vogl_new(vogl_ctypes);

    if (m_pTrace_ctypes == NULL)
    {
        return false;
    }

    m_pTrace_ctypes->init(pTrace_reader->get_sof_packet().m_pointer_sizes);

    for ( ; ; )
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogleditor_output_error("Failed reading from trace file!");
            return false;
        }

        if (read_status == vogl_trace_file_reader::cEOF)
        {
            vogl_printf("At trace file EOF on swap %" PRIu64 "\n", total_swaps);
            return false;
        }

        const vogl::vector<uint8_t> &packet_buf = pTrace_reader->get_packet_buf(); VOGL_NOTE_UNUSED(packet_buf);

        const vogl_trace_stream_packet_base &base_packet = pTrace_reader->get_base_packet(); VOGL_NOTE_UNUSED(base_packet);
        const vogl_trace_gl_entrypoint_packet *pGL_packet = NULL;

        if (pTrace_reader->get_packet_type() == cTSPTGLEntrypoint)
        {
            vogl_trace_packet* pTrace_packet = vogl_new(vogl_trace_packet, m_pTrace_ctypes);

            if (!pTrace_packet->deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
            {
                vogleditor_output_error("Failed parsing GL entrypoint packet.");
                return false;
            }

            if (!pTrace_packet->check())
            {
                vogleditor_output_error("GL entrypoint packet failed"
                " consistency check. Please make sure the trace was made with"
                " the most recent version of VOGL.");
                return false;
            }

            pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
            gl_entrypoint_id_t entrypoint_id = pTrace_packet->get_entrypoint_id();

            if (entrypoint_id == VOGL_ENTRYPOINT_glInternalTraceCommandRAD)
            {
                // Check if this is a state snapshot.
                // This is entirely optional since the client is designed to dynamically get new snapshots
                // if they don't exist.
                GLuint cmd = pTrace_packet->get_param_value<GLuint>(0);
                GLuint size = pTrace_packet->get_param_value<GLuint>(1); VOGL_NOTE_UNUSED(size);

                if (cmd == cITCRKeyValueMap)
                {
                    key_value_map &kvm = pTrace_packet->get_key_value_map();

                    dynamic_string cmd_type(kvm.get_string("command_type"));
                    if (cmd_type == "state_snapshot")
                    {
                        dynamic_string id(kvm.get_string("binary_id"));
                        if (id.is_empty())
                        {
                            vogl_warning_printf("Missing binary_id field in glInternalTraceCommandRAD key_value_map command type: \"%s\"\n", cmd_type.get_ptr());
                            continue;
                        }

                        uint8_vec snapshot_data;
                        {
                            timed_scope ts("get_multi_blob_manager().get");
                            if (!pTrace_reader->get_multi_blob_manager().get(id, snapshot_data) || (snapshot_data.is_empty()))
                            {
                                vogl_warning_printf("Failed reading snapshot blob data \"%s\"!\n", id.get_ptr());
                                continue;
                            }
                        }

                        json_document doc;
                        {
                            timed_scope ts("doc.binary_deserialize");
                            if (!doc.binary_deserialize(snapshot_data) || (!doc.get_root()))
                            {
                                vogl_warning_printf("Failed deserializing JSON snapshot blob data \"%s\"!\n", id.get_ptr());
                                continue;
                            }
                        }

                        vogl_gl_state_snapshot* pGLSnapshot = vogl_new(vogl_gl_state_snapshot);
                        pPendingSnapshot = vogl_new(vogleditor_gl_state_snapshot, pGLSnapshot);

                        timed_scope ts("pPendingSnapshot->deserialize");
                        if (!pPendingSnapshot->get_snapshot()->deserialize(*doc.get_root(), pTrace_reader->get_multi_blob_manager(), m_pTrace_ctypes))
                        {
                            vogl_delete(pPendingSnapshot);
                            pPendingSnapshot = NULL;

                            vogl_warning_printf("Failed deserializing snapshot blob data \"%s\"!\n", id.get_ptr());
                        }
                    }
                }

                continue;
            }

            // if we don't have a current frame, make a new frame node
            // and append it to the pParentRoot
            if (pCurFrame == NULL)
            {
                pCurFrame = vogl_new(vogleditor_frameItem, total_swaps);
                vogleditor_apiCallTreeItem* pNewFrameNode = vogl_new(vogleditor_apiCallTreeItem, pCurFrame, pParentRoot);
                pParentRoot->appendChild(pNewFrameNode);
                m_itemList.append(pNewFrameNode);

                if (pPendingSnapshot != NULL)
                {
                    pCurFrame->set_snapshot(pPendingSnapshot);
                    pPendingSnapshot = NULL;
                }

                // update current parent
                pCurParent = pNewFrameNode;

            } // pCurFrame == NULL

            // Pre-process apiCall
            // -------------------
            if (pCurParent->isFrame())
            {
                // Frame treenode children should only consist of container
                // treenodes, i.e., state/render groups or marker_push types 
                // (marker_push types [e.g., glPushDebugGroups] are post-
                // processed so skip them here)
                if ( ! vogl_is_marker_push_entrypoint(entrypoint_id))
                {
                    // Start a new state/render group container
                    pCurParent = create_group(pCurFrame, pCurGroup, pCurParent);
                }
            } // pCurParent->isFrame()

            else if (vogl_is_marker_push_entrypoint(entrypoint_id))
            {
                if (pCurParent->isGroup())
                {
                    pCurParent = pCurParent->parent();
                }
            } // vogl_is_marker_push_entrypoint(entrypoint_id)

            else if (vogl_is_start_nested_entrypoint(entrypoint_id))
            {
                if (pCurParent->isGroup()) // state/render group?
                {
                    if (pCurParent->childCount() != 0)
                    {
                        // TODO: Maybe think about a more unique name so as not
                        //       to be confused with a marker push type apicall
                        //       that has been named "Render"
                        //
                        // If we're in a series of start/end nests, following
                        // will be skipped to stay in current group; Otherwise
                        // current group ends and a new group is started
                        if (!vogl_is_end_nested_entrypoint(lastItemApiCallId()))
                        {
                            pCurParent = pCurParent->parent();
                            pCurParent = create_group(pCurFrame, pCurGroup, pCurParent);
                        }
                    }
                }
            }
            // For Render groups that have nested calls in which the end_nested
            // entrypoint is also a frame_buffer_write entypoint (e.g.,
            // glBegin/End): If we get here, we know entrypoint_id is not a
            // start_nested entrypoint (processed in above block) so check if
            // previous call was a frame_buffer_write entrypoint (and by
            // definition also an end_nested). If so then we've broken any
            // series (one or more) of sequential start/end nests and now close.
            else if (vogl_is_frame_buffer_write_entrypoint(lastItemApiCallId()))
            {
                if (pCurParent->isGroup())
                {
                    pCurParent = pCurParent->parent();
                    pCurParent = create_group(pCurFrame, pCurGroup, pCurParent);
                }
            }

            // Process apiCall
            // ---------------
            vogleditor_apiCallItem *pCallItem = NULL;
            vogleditor_apiCallTreeItem  *item = NULL;

            // Remove following if-statement to allow marker_pop_entrypoint
            // apicalls (e.g., glPopDebugGroup) to be added to the apicall tree.
            if (!vogl_is_marker_pop_entrypoint(entrypoint_id))
            {
                // make item for the api call
                pCallItem = vogl_new(vogleditor_apiCallItem, pCurFrame, pTrace_packet, *pGL_packet);
                pCurFrame->appendCall(pCallItem);
                if (pCurParent->isGroup())
                {
                    pCurGroup->appendCall(pCallItem);
                }

                if (pPendingSnapshot != NULL)
                {
                    pCallItem->set_snapshot(pPendingSnapshot);
                    pPendingSnapshot = NULL;
                }

                // make node for the api call
                item = vogl_new(vogleditor_apiCallTreeItem, pCallItem, pCurParent);
                pCurParent->appendChild(item);
                m_itemList.append(item);
            }

            // Post-process apiCall
            // --------------------
            if (vogl_is_swap_buffers_entrypoint(entrypoint_id))
            {
                total_swaps++;

                // reset the CurParent back to the original parent so that the next frame will be at the root level
                pCurParent = pParentRoot;

                // reset the CurFrame so that a new frame node will be created on the next api call
                pCurFrame = NULL;
            }
            else if (vogl_is_start_nested_entrypoint(entrypoint_id))
            {
                // start a new nested layer with this item as parent
                pCurParent = item;
            }
            else if (vogl_is_end_nested_entrypoint(entrypoint_id))
            {
                if (!pCurParent->isFrame())
                    pCurParent = pCurParent->parent();
            }
            else if (vogl_is_marker_push_entrypoint(entrypoint_id))
            {
                // Append a new state/render group child to marker_push
                // entrypoint item and make it the parent
                pCurParent = item;
            }
            else if (vogl_is_marker_pop_entrypoint(entrypoint_id))
            {
                // move parent up one level (but not past Frame parent)
                // [e.g., if there was no corresponding marker_push]
                if (!pCurParent->isFrame())
                {
                    // Rename (marker) parent tree node
                    QString name = pCurParent->markerApiCallDebugMessage();
                    if (!name.isEmpty())
                    {
                        name = "\"" + name + "\"" + " group";
                        pCurParent->setApiCallColumnData(QVariant(name));
                    }

                    // End marker push parent container and move pCurParent back
                    // up to parent of current (marker push) parent - unless
                    // this is a sequence of nested apicalls (e.g., glBegin/End)
                    // which then will be delayed until end of series
                    //if (!vogl_is_end_nested_entrypoint(lastItemApiCallId()))
                    {
                        pCurParent = pCurParent->parent();
                    }
                }
            } // vogl_is_marker_pop_entrypoint(entrypoint_id)

            // end group on draw and start new group
            if (vogl_is_frame_buffer_write_entrypoint(entrypoint_id))
            {
                // set group name to "Render"
                if (pCurParent->isGroup())
                {
                    // Do check in case of serial end_nested calls that are also
                    // frame_buffer_writes and only set once
                    if (pCurParent->apiCallColumnData() != cTREEITEM_RENDER)
                    {
                        // Set group node column data
                        pCurParent->setApiCallColumnData(cTREEITEM_RENDER);
                    }
                }
            } // vogl_is_frame_buffer_write_entrypoint(entrypoint_id)
        } // if cTSPTGLEntrypoint

        if (pTrace_reader->get_packet_type() == cTSPTEOF)
        {
            found_eof_packet = true;
            vogl_printf("Found trace file EOF packet on swap %" PRIu64 "\n", total_swaps);
            break;
        }
    }

    return found_eof_packet;
}

gl_entrypoint_id_t vogleditor_QApiCallTreeModel::itemApiCallId(vogleditor_apiCallTreeItem *apiCallTreeItem) const
{
    gl_entrypoint_id_t callId = VOGL_ENTRYPOINT_INVALID;
    if (apiCallTreeItem)
    {
        vogleditor_apiCallItem *callItem = apiCallTreeItem->apiCallItem();
        if (callItem)
        {
            callId = callItem->getTracePacket()->get_entrypoint_id();
        }
    }
    return callId;
}

gl_entrypoint_id_t vogleditor_QApiCallTreeModel::lastItemApiCallId() const
{
    gl_entrypoint_id_t id = VOGL_ENTRYPOINT_INVALID;
    if (!m_itemList.isEmpty())
    {
        id = itemApiCallId(m_itemList.last());
    }
    return id;
}

void vogleditor_QApiCallTreeModel::delete_group(vogleditor_frameItem  *pCurFrameObj,
                                                vogleditor_apiCallTreeItem *&pParentNode)
{
    // Assumptions 
    // ------------
    // pParentNode is a group tree item type of vogleditor_apiCallTreeItem 
    //
    // pParentNode was the last added group item to the frame's group item list
    //
    // Grandparent (pParentNode->parent()) is a marker_push_entrypoint type
    //
    // TODO: add checks for the assumptions (and pCurParent has no children..?)

    // Remove parent frame's last appended group item (vogleditor_groupItem)
    pCurFrameObj->popGroup();

    // Clear pParentNode (of type "group" apiCallTreeItem) from grandparent
    // (marker_push_entrypoint type) and this QApiCallTreeModel's m_itemList
    pParentNode->parent()->popChild();
    m_itemList.removeLast();

    // Set pCurParent to pCurParent->parent()
    vogleditor_apiCallTreeItem *pOldParent = pParentNode;
    pParentNode = pParentNode->parent();

    // Delete old pCurParent
    vogl_delete(pOldParent);
}

vogleditor_apiCallTreeItem *vogleditor_QApiCallTreeModel::create_group(vogleditor_frameItem  *pCurFrameObj,
                                                                       vogleditor_groupItem *&pCurGroupObj,
                                                                       vogleditor_apiCallTreeItem *pParentNode)
{
// Operations:
//    . create a groupItem object
//      - add to current frame's groupItem list
// 
//    . create "group" tree item type (apiCallTreeItem)
//      - add to parent tree item
//      - add to this object's apiCallItem list (m_itemList)

    // Make a new group item
    pCurGroupObj = vogl_new(vogleditor_groupItem, pCurFrameObj);
    pCurFrameObj->appendGroup(pCurGroupObj); // (move this to constructor?)

    // Make a new group tree item and insert into tree
    vogleditor_apiCallTreeItem *pNewGroupNode = vogl_new(vogleditor_apiCallTreeItem, pCurGroupObj, pParentNode);
    pParentNode->appendChild(pNewGroupNode);
    m_itemList.append(pNewGroupNode);
    return pNewGroupNode;
}

QModelIndex vogleditor_QApiCallTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    vogleditor_apiCallTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<vogleditor_apiCallTreeItem*>(parent.internalPointer());

    vogleditor_apiCallTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex vogleditor_QApiCallTreeModel::indexOf(const vogleditor_apiCallTreeItem* pItem) const
{
    if (pItem != NULL)
        return createIndex(pItem->row(), VOGL_ACTC_APICALL, (void*)pItem);
    else
        return QModelIndex();
}

QModelIndex vogleditor_QApiCallTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    vogleditor_apiCallTreeItem* childItem = static_cast<vogleditor_apiCallTreeItem*>(index.internalPointer());
    vogleditor_apiCallTreeItem* parentItem = childItem->parent();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), VOGL_ACTC_APICALL, parentItem);
}

int vogleditor_QApiCallTreeModel::rowCount(const QModelIndex &parent) const
{
    vogleditor_apiCallTreeItem* parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<vogleditor_apiCallTreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int vogleditor_QApiCallTreeModel::columnCount(const QModelIndex &parent) const
{
    VOGL_NOTE_UNUSED(parent);
    return VOGL_MAX_ACTC;
}

QVariant vogleditor_QApiCallTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    vogleditor_apiCallTreeItem* pItem = static_cast<vogleditor_apiCallTreeItem*>(index.internalPointer());

    if (pItem == NULL)
    {
        return QVariant();
    }

    // make draw call rows appear in bold
    if (role == Qt::FontRole && pItem->apiCallItem() != NULL && vogl_is_frame_buffer_write_entrypoint((gl_entrypoint_id_t)pItem->apiCallItem()->getGLPacket()->m_entrypoint_id))
    {
        QFont font;
        font.setBold(true);
        return font;
    }

    // highlight the API call cell if it has a substring which matches the searchString
    if (role == Qt::BackgroundRole && index.column() == VOGL_ACTC_APICALL)
    {
        if (!m_searchString.isEmpty())
        {
            QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
            QString string = data.toString();
            if (string.contains(m_searchString, Qt::CaseInsensitive))
            {
                return QColor(Qt::yellow);
            }
        }
    }

    return pItem->columnData(index.column(), role);
}

Qt::ItemFlags vogleditor_QApiCallTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant vogleditor_QApiCallTreeModel::headerData(int section, Qt::Orientation orientation,
                                                  int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_rootItem->columnData(section, role);

    return QVariant();
}

void vogleditor_QApiCallTreeModel::set_highlight_search_string(const QString searchString)
{
    m_searchString = searchString;
}

QModelIndex vogleditor_QApiCallTreeModel::find_prev_search_result(vogleditor_apiCallTreeItem* start, const QString searchText)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list, so return a default (invalid) item
            return QModelIndex();
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekPrevious();
        QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
        QString string = data.toString();
        if (string.contains(searchText, Qt::CaseInsensitive))
        {
            pFound = pItem;
            break;
        }

        iter.previous();
    }

    return indexOf(pFound);
}

QModelIndex vogleditor_QApiCallTreeModel::find_next_search_result(vogleditor_apiCallTreeItem* start, const QString searchText)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list, so return a default (invalid) item
            return QModelIndex();
        }
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
        QString string = data.toString();
        if (string.contains(searchText, Qt::CaseInsensitive))
        {
            pFound = pItem;
            break;
        }

        iter.next();
    }

    return indexOf(pFound);
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_prev_snapshot(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        if (iter.peekPrevious()->has_snapshot())
        {
            pFound = iter.peekPrevious();
            break;
        }

        iter.previous();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_next_snapshot(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    // if start is NULL, then search will begin from top, otherwise it will begin from the start item and search onwards
    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        if (iter.peekNext()->has_snapshot())
        {
            pFound = iter.peekNext();
            break;
        }

        iter.next();
    }

    return pFound;
}


vogleditor_apiCallTreeItem *vogleditor_QApiCallTreeModel::find_prev_drawcall(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekPrevious();
        if (pItem->apiCallItem() != NULL)
        {
            gl_entrypoint_id_t entrypointId = pItem->apiCallItem()->getTracePacket()->get_entrypoint_id();
            if (vogl_is_frame_buffer_write_entrypoint(entrypointId))
            {
                pFound = iter.peekPrevious();
                break;
            }
        }

        iter.previous();
    }

    return pFound;
}

vogleditor_apiCallTreeItem *vogleditor_QApiCallTreeModel::find_next_drawcall(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (iter.findNext(start) == false)
    {
        // the object wasn't found in the list
        return NULL;
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->apiCallItem() != NULL)
        {
            gl_entrypoint_id_t entrypointId = pItem->apiCallItem()->getTracePacket()->get_entrypoint_id();
            if (vogl_is_frame_buffer_write_entrypoint(entrypointId))
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_call_number(unsigned int callNumber)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->apiCallItem() != NULL)
        {
            if (pItem->apiCallItem()->globalCallIndex() == callNumber)
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_frame_number(unsigned int frameNumber)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->frameItem() != NULL)
        {
            if (pItem->frameItem()->frameNumber() == frameNumber)
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}
