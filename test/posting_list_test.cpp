#include <gtest/gtest.h>
#include "posting.h"
#include "array_utils.h"
#include <chrono>
#include <vector>

TEST(PostingListTest, Insert) {
    std::vector<uint32_t> offsets = {0, 1, 3};

    posting_list_t pl(5);

    // insert elements sequentially

    for(size_t i = 0; i < 15; i++) {
        pl.upsert(i, offsets);
    }

    posting_list_t::block_t* root = pl.get_root();
    ASSERT_EQ(5, root->ids.getLength());
    ASSERT_EQ(5, root->next->ids.getLength());
    ASSERT_EQ(5, root->next->next->ids.getLength());

    ASSERT_EQ(root->next->next->next, nullptr);

    ASSERT_EQ(3, pl.size());
    ASSERT_EQ(root, pl.block_of(4));
    ASSERT_EQ(root->next, pl.block_of(9));
    ASSERT_EQ(root->next->next, pl.block_of(14));

    // insert alternate values

    posting_list_t pl2(5);

    for(size_t i = 0; i < 15; i+=2) {
        // [0, 2, 4, 6, 8], [10, 12, 14]
        pl2.upsert(i, offsets);
    }

    root = pl2.get_root();
    ASSERT_EQ(5, root->ids.getLength());
    ASSERT_EQ(3, root->next->ids.getLength());

    ASSERT_EQ(root->next->next, nullptr);
    ASSERT_EQ(2, pl2.size());

    ASSERT_EQ(root, pl2.block_of(8));
    ASSERT_EQ(root->next, pl2.block_of(14));

    // insert in the middle
    // case 1

    posting_list_t pl3(5);

    for(size_t i = 0; i < 5; i++) {
        pl3.upsert(i, offsets);
    }

    pl3.upsert(6, offsets);
    pl3.upsert(8, offsets);
    pl3.upsert(9, offsets);
    pl3.upsert(10, offsets);
    pl3.upsert(12, offsets);

    // [0,1,2,3,4], [6,8,9,10,12]
    pl3.upsert(5, offsets);
    ASSERT_EQ(3, pl3.size());
    ASSERT_EQ(5, pl3.get_root()->ids.getLength());
    ASSERT_EQ(3, pl3.get_root()->next->ids.getLength());
    ASSERT_EQ(8, pl3.get_root()->next->ids.last());
    ASSERT_EQ(3, pl3.get_root()->next->next->ids.getLength());
    ASSERT_EQ(12, pl3.get_root()->next->next->ids.last());

    for(size_t i = 0; i < pl3.get_root()->next->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl3.get_root()->next->offset_index.at(i));
    }

    for(size_t i = 0; i < pl3.get_root()->next->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl3.get_root()->next->offsets.at(i));
    }

    // case 2
    posting_list_t pl4(5);

    for(size_t i = 0; i < 5; i++) {
        pl4.upsert(i, offsets);
    }

    pl4.upsert(6, offsets);
    pl4.upsert(8, offsets);
    pl4.upsert(9, offsets);
    pl4.upsert(10, offsets);
    pl4.upsert(12, offsets);

    // [0,1,2,3,4], [6,8,9,10,12]
    pl4.upsert(11, offsets);
    ASSERT_EQ(3, pl4.size());

    ASSERT_EQ(5, pl4.get_root()->ids.getLength());
    ASSERT_EQ(3, pl4.get_root()->next->ids.getLength());
    ASSERT_EQ(9, pl4.get_root()->next->ids.last());
    ASSERT_EQ(3, pl4.get_root()->next->next->ids.getLength());
    ASSERT_EQ(12, pl4.get_root()->next->next->ids.last());

    for(size_t i = 0; i < pl4.get_root()->next->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl4.get_root()->next->offset_index.at(i));
    }

    for(size_t i = 0; i < pl4.get_root()->next->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl4.get_root()->next->offsets.at(i));
    }
}

TEST(PostingListTest, RemovalsOnFirstBlock) {
    std::vector<uint32_t> offsets = {0, 1, 3};
    posting_list_t pl(5);

    ASSERT_EQ(0, pl.size());

    // try to erase when posting list is empty
    pl.erase(0);

    ASSERT_EQ(0, pl.size());

    // insert a single element and erase it
    pl.upsert(0, offsets);
    ASSERT_EQ(1, pl.size());
    pl.erase(0);
    ASSERT_EQ(0, pl.size());

    ASSERT_EQ(0, pl.get_root()->ids.getLength());
    ASSERT_EQ(0, pl.get_root()->offset_index.getLength());
    ASSERT_EQ(0, pl.get_root()->offsets.getLength());

    // insert until one past max block size
    for(size_t i = 0; i < 6; i++) {
        pl.upsert(i, offsets);
    }

    ASSERT_EQ(2, pl.size());

    // delete non-existing element
    pl.erase(1000);

    // delete elements from first block: blocks should not be merged until it falls below 50% occupancy
    pl.erase(1);
    ASSERT_EQ(2, pl.size());

    // [0, 2, 3, 4], [5]

    for(size_t i = 0; i < pl.get_root()->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->offsets.at(i));
    }

    pl.erase(2);
    ASSERT_EQ(2, pl.size());
    pl.erase(3);

    // [0, 4], [5]
    ASSERT_EQ(2, pl.size());
    ASSERT_EQ(2, pl.get_root()->size());
    ASSERT_EQ(1, pl.get_root()->next->size());
    ASSERT_EQ(pl.get_root(), pl.block_of(4));
    ASSERT_EQ(pl.get_root()->next, pl.block_of(5));

    for(size_t i = 0; i < pl.get_root()->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->offsets.at(i));
    }

    pl.erase(4);  // this will trigger the merge

    // [0, 5]
    // ensure that merge has happened
    ASSERT_EQ(1, pl.size());
    ASSERT_EQ(pl.get_root(), pl.block_of(5));
    ASSERT_EQ(nullptr, pl.get_root()->next);
    ASSERT_EQ(2, pl.get_root()->size());

    for(size_t i = 0; i < pl.get_root()->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->offsets.at(i));
    }
}

TEST(PostingListTest, RemovalsOnLaterBlocks) {
    std::vector<uint32_t> offsets = {0, 1, 3};
    posting_list_t pl(5);

    // insert until one past max block size
    for(size_t i = 0; i < 6; i++) {
        pl.upsert(i, offsets);
    }

    // erase last element of last, non-first block

    pl.erase(5);
    ASSERT_EQ(1, pl.size());
    ASSERT_EQ(5, pl.get_root()->size());
    ASSERT_EQ(4, pl.get_root()->ids.last());
    ASSERT_EQ(nullptr, pl.get_root()->next);

    for(size_t i = 0; i < pl.get_root()->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->offsets.at(i));
    }

    // erase last element of the only block when block is atleast half full
    pl.erase(4);
    ASSERT_EQ(1, pl.size());
    ASSERT_EQ(4, pl.get_root()->size());
    ASSERT_EQ(3, pl.get_root()->ids.last());
    ASSERT_EQ(pl.get_root(), pl.block_of(3));

    for(size_t i = 4; i < 15; i++) {
        pl.upsert(i, offsets);
    }

    // [0..4], [5..9], [10..14]
    pl.erase(5);
    pl.erase(6);
    pl.erase(7);

    for(size_t i = 0; i < pl.get_root()->next->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->next->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->next->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->next->offsets.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->next->next->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->next->next->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->next->next->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->next->next->offsets.at(i));
    }

    // only part of the next node contents can be moved over when we delete 8 since (1 + 5) > 5
    pl.erase(8);

    // [0..4], [9], [10..14] => [0..4], [9,10,11,12,13], [14]

    ASSERT_EQ(3, pl.size());
    ASSERT_EQ(5, pl.get_root()->next->size());
    ASSERT_EQ(1, pl.get_root()->next->next->size());
    ASSERT_EQ(13, pl.get_root()->next->ids.last());
    ASSERT_EQ(14, pl.get_root()->next->next->ids.last());

    for(size_t i = 0; i < pl.get_root()->next->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->next->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->next->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->next->offsets.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->next->next->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->next->next->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->next->next->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->next->next->offsets.at(i));
    }
}

TEST(PostingListTest, OutOfOrderUpserts) {
    std::vector<uint32_t> offsets = {0, 1, 3};
    posting_list_t pl(5);

    for(int i = 5; i > 0; i--) {
        pl.upsert(i, offsets);
    }

    pl.upsert(0, offsets);
    pl.upsert(200000, offsets);

    ASSERT_EQ(2, pl.size());

    ASSERT_EQ(3, pl.get_root()->size());
    ASSERT_EQ(4, pl.get_root()->next->size());

    for(size_t i = 0; i < pl.get_root()->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->offsets.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->next->offset_index.getLength(); i++) {
        ASSERT_EQ(i * 3, pl.get_root()->next->offset_index.at(i));
    }

    for(size_t i = 0; i < pl.get_root()->next->offsets.getLength(); i++) {
        ASSERT_EQ(offsets[i % 3], pl.get_root()->next->offsets.at(i));
    }
}

TEST(PostingListTest, RandomInsertAndDeletes) {
    time_t t;
    srand((unsigned) time(&t));

    posting_list_t pl(100);
    std::vector<uint32_t> offsets1 = {0, 1, 3};
    std::vector<uint32_t> offsets2 = {10, 12};

    // generate unique random IDs
    std::set<uint32_t> ids;

    for(size_t i = 0; i < 100000; i++) {
        ids.insert(rand() % 100000);
    }

    size_t index = 0;

    for(auto id: ids) {
        const std::vector<uint32_t>& offsets = (index % 2 == 0) ? offsets1 : offsets2;
        pl.upsert(id, offsets);
        index++;
    }

    for(size_t i = 0; i < 10000; i++) {
        pl.erase(rand() % 100000);
    }

    ASSERT_LT(pl.size(), 750);
    ASSERT_GT(pl.size(), 500);
}

TEST(PostingListTest, IntersectionBasics) {
    std::vector<uint32_t> offsets = {0, 1, 3};
    std::vector<posting_list_t*> lists;

    // [0, 2] [3, 20]
    // [1, 3], [5, 10], [20]
    // [2, 3], [5, 7], [20]

    posting_list_t p1(2);
    p1.upsert(0, offsets);
    p1.upsert(2, offsets);
    p1.upsert(3, offsets);
    p1.upsert(20, offsets);

    posting_list_t p2(2);
    p2.upsert(1, offsets);
    p2.upsert(3, offsets);
    p2.upsert(5, offsets);
    p2.upsert(10, offsets);
    p2.upsert(20, offsets);

    posting_list_t p3(2);
    p3.upsert(2, offsets);
    p3.upsert(3, offsets);
    p3.upsert(5, offsets);
    p3.upsert(7, offsets);
    p3.upsert(20, offsets);

    lists.push_back(&p1);
    lists.push_back(&p2);
    lists.push_back(&p3);

    std::vector<uint32_t> result_ids;

    posting_list_t::intersect(lists, result_ids);

    ASSERT_EQ(2, result_ids.size());
    ASSERT_EQ(3, result_ids[0]);
    ASSERT_EQ(20, result_ids[1]);
}

TEST(PostingListTest, IntersectionSkipBlocks) {
    std::vector<uint32_t> offsets = {0, 1, 3};
    std::vector<posting_list_t*> lists;

    std::vector<uint32_t> p1_ids = {9, 11};
    std::vector<uint32_t> p2_ids = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11};
    std::vector<uint32_t> p3_ids = {2, 3, 8, 9, 11, 20};

    // [9, 11]
    // [1, 2], [3, 4], [5, 6], [7, 8], [9, 11]
    // [2, 3], [8, 9], [11, 20]

    posting_list_t p1(2);
    posting_list_t p2(2);
    posting_list_t p3(2);

    for(auto id: p1_ids) {
        p1.upsert(id, offsets);
    }

    for(auto id: p2_ids) {
        p2.upsert(id, offsets);
    }

    for(auto id: p3_ids) {
        p3.upsert(id, offsets);
    }

    lists.push_back(&p1);
    lists.push_back(&p2);
    lists.push_back(&p3);

    std::vector<uint32_t> result_ids;
    posting_list_t::intersect(lists, result_ids);

    uint32_t* p1_p2_intersected;
    uint32_t* final_results;

    size_t temp_len = ArrayUtils::and_scalar(&p1_ids[0], p1_ids.size(), &p2_ids[0], p2_ids.size(), &p1_p2_intersected);
    size_t final_len = ArrayUtils::and_scalar(&p3_ids[0], p3_ids.size(), p1_p2_intersected, temp_len, &final_results);

    ASSERT_EQ(final_len, result_ids.size());

    for(size_t i = 0; i < result_ids.size(); i++) {
        ASSERT_EQ(final_results[i], result_ids[i]);
    }

    delete [] p1_p2_intersected;
    delete [] final_results;
}

TEST(PostingListTest, CompactPostingListUpsertAppends) {
    uint32_t ids[] = {0, 1000, 1002};
    uint32_t offset_index[] = {0, 3, 6};
    uint32_t offsets[] = {0, 3, 4, 0, 3, 4, 0, 3, 4};

    compact_posting_list_t* list = compact_posting_list_t::create(3, ids, offset_index, 9, offsets);
    ASSERT_EQ(15, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());

    // no-op since the container expects resizing to be done outside
    list->upsert(1003, {1, 2});
    ASSERT_EQ(15, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());

    // now resize
    void* obj = SET_COMPACT_POSTING(list);
    posting_t::upsert(obj, 1003, {1, 2});
    ASSERT_EQ(1003, COMPACT_POSTING_PTR(obj)->last_id());

    ASSERT_EQ(19, (COMPACT_POSTING_PTR(obj))->length);
    ASSERT_EQ(24, (COMPACT_POSTING_PTR(obj))->capacity);

    // insert enough docs to NOT exceed compact posting list threshold
    posting_t::upsert(obj, 1004, {1, 2, 3, 4, 5, 6, 7, 8});
    ASSERT_EQ(1004, COMPACT_POSTING_PTR(obj)->last_id());
    posting_t::upsert(obj, 1005, {1, 2, 3, 4, 5, 6, 7, 8});
    ASSERT_EQ(1005, COMPACT_POSTING_PTR(obj)->last_id());
    posting_t::upsert(obj, 1006, {1, 2, 3, 4, 5, 6, 7, 8});
    ASSERT_EQ(1006, COMPACT_POSTING_PTR(obj)->last_id());
    posting_t::upsert(obj, 1007, {1, 2, 3, 4, 5, 6, 7, 8});
    ASSERT_EQ(1007, COMPACT_POSTING_PTR(obj)->last_id());
    ASSERT_TRUE(IS_COMPACT_POSTING(obj));
    ASSERT_EQ(1007, COMPACT_POSTING_PTR(obj)->last_id());

    // next upsert will exceed threshold
    posting_t::upsert(obj, 1008, {1, 2, 3, 4, 5, 6, 7, 8});
    ASSERT_FALSE(IS_COMPACT_POSTING(obj));

    ASSERT_EQ(1, ((posting_list_t*)(obj))->size());
    ASSERT_EQ(9, ((posting_list_t*)(obj))->get_root()->size());
    ASSERT_EQ(1008, ((posting_list_t*)(obj))->get_root()->ids.last());

    delete ((posting_list_t*)(obj));
}

TEST(PostingListTest, CompactPostingListUpserts) {
    uint32_t ids[] = {3, 1000, 1002};
    uint32_t offset_index[] = {0, 3, 6};
    uint32_t offsets[] = {0, 3, 4, 0, 3, 4, 0, 3, 4};

    compact_posting_list_t* list = compact_posting_list_t::create(3, ids, offset_index, 9, offsets);
    ASSERT_EQ(15, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());

    // insert before first ID

    void* obj = SET_COMPACT_POSTING(list);
    posting_t::upsert(obj, 2, {1, 2});
    ASSERT_EQ(1002, COMPACT_POSTING_PTR(obj)->last_id());
    ASSERT_EQ(19, COMPACT_POSTING_PTR(obj)->length);
    ASSERT_EQ(24, COMPACT_POSTING_PTR(obj)->capacity);

    // insert in the middle
    posting_t::upsert(obj, 999, {1, 2});
    ASSERT_EQ(1002, COMPACT_POSTING_PTR(obj)->last_id());
    ASSERT_EQ(23, COMPACT_POSTING_PTR(obj)->length);
    ASSERT_EQ(24, COMPACT_POSTING_PTR(obj)->capacity);

    uint32_t expected_id_offsets[] = {
        2, 1, 2, 2,
        3, 0, 3, 4, 3,
        2, 1, 2, 999,
        3, 0, 3, 4, 1000,
        3, 0, 3, 4, 1002
    };

    ASSERT_EQ(23, COMPACT_POSTING_PTR(obj)->length);

    for(size_t i = 0; i < COMPACT_POSTING_PTR(obj)->length; i++) {
        ASSERT_EQ(expected_id_offsets[i], COMPACT_POSTING_PTR(obj)->id_offsets[i]);
    }

    free(COMPACT_POSTING_PTR(obj));
}

TEST(PostingListTest, CompactPostingListUpdateWithLessOffsets) {
    uint32_t ids[] = {0, 1000, 1002};
    uint32_t offset_index[] = {0, 3, 6};
    uint32_t offsets[] = {0, 3, 4, 0, 3, 4, 0, 3, 4};

    compact_posting_list_t* list = compact_posting_list_t::create(3, ids, offset_index, 9, offsets);
    ASSERT_EQ(15, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());

    // update middle

    list->upsert(1000, {1, 2});
    ASSERT_EQ(14, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());
    uint32_t expected_id_offsets[] = {3, 0, 3, 4, 0, 2, 1, 2, 1000, 3, 0, 3, 4, 1002};
    for(size_t i = 0; i < list->length; i++) {
        ASSERT_EQ(expected_id_offsets[i], list->id_offsets[i]);
    }

    // update start
    list->upsert(0, {2, 4});
    ASSERT_EQ(13, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());
    uint32_t expected_id_offsets2[] = {2, 2, 4, 0, 2, 1, 2, 1000, 3, 0, 3, 4, 1002};
    for(size_t i = 0; i < list->length; i++) {
        ASSERT_EQ(expected_id_offsets2[i], list->id_offsets[i]);
    }

    // update end
    list->upsert(1002, {2, 4});
    ASSERT_EQ(12, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());
    uint32_t expected_id_offsets3[] = {2, 2, 4, 0, 2, 1, 2, 1000, 2, 2, 4, 1002};
    for(size_t i = 0; i < list->length; i++) {
        ASSERT_EQ(expected_id_offsets3[i], list->id_offsets[i]);
    }

    free(list);
}

TEST(PostingListTest, CompactPostingListUpdateWithMoreOffsets) {
    uint32_t ids[] = {0, 1000, 1002};
    uint32_t offset_index[] = {0, 3, 6};
    uint32_t offsets[] = {0, 3, 4, 0, 3, 4, 0, 3, 4};

    compact_posting_list_t* list = compact_posting_list_t::create(3, ids, offset_index, 9, offsets);
    ASSERT_EQ(15, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());

    // update middle
    void* obj = SET_COMPACT_POSTING(list);
    posting_t::upsert(obj, 1000, {1, 2, 3, 4});
    list = COMPACT_POSTING_PTR(obj);
    ASSERT_EQ(16, list->length);
    ASSERT_EQ(20, list->capacity);
    ASSERT_EQ(1002, list->last_id());
    uint32_t expected_id_offsets[] = {3, 0, 3, 4, 0, 4, 1, 2, 3, 4, 1000, 3, 0, 3, 4, 1002};
    for(size_t i = 0; i < list->length; i++) {
        ASSERT_EQ(expected_id_offsets[i], list->id_offsets[i]);
    }

    // update start
    list->upsert(0, {1, 2, 3, 4});
    ASSERT_EQ(17, list->length);
    ASSERT_EQ(20, list->capacity);
    ASSERT_EQ(1002, list->last_id());
    uint32_t expected_id_offsets2[] = {4, 1, 2, 3, 4, 0, 4, 1, 2, 3, 4, 1000, 3, 0, 3, 4, 1002};
    for(size_t i = 0; i < list->length; i++) {
        ASSERT_EQ(expected_id_offsets2[i], list->id_offsets[i]);
    }

    // update end
    list->upsert(1002, {1, 2, 3, 4});
    ASSERT_EQ(18, list->length);
    ASSERT_EQ(20, list->capacity);
    ASSERT_EQ(1002, list->last_id());
    uint32_t expected_id_offsets3[] = {4, 1, 2, 3, 4, 0, 4, 1, 2, 3, 4, 1000, 4, 1, 2, 3, 4, 1002};
    for(size_t i = 0; i < list->length; i++) {
        ASSERT_EQ(expected_id_offsets3[i], list->id_offsets[i]);
    }

    free(list);
}

TEST(PostingListTest, CompactPostingListErase) {
    uint32_t ids[] = {0, 1000, 1002};
    uint32_t offset_index[] = {0, 3, 6};
    uint32_t offsets[] = {0, 3, 4, 0, 3, 4, 0, 3, 4};

    compact_posting_list_t* list = compact_posting_list_t::create(3, ids, offset_index, 9, offsets);

    list->erase(3); // erase non-existing ID

    ASSERT_EQ(15, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());

    list->erase(1000);
    ASSERT_EQ(10, list->length);
    ASSERT_EQ(15, list->capacity);
    ASSERT_EQ(1002, list->last_id());

    // deleting using posting wrapper
    void* obj = SET_COMPACT_POSTING(list);
    posting_t::erase(obj, 1002);
    ASSERT_TRUE(IS_COMPACT_POSTING(obj));
    ASSERT_EQ(5, (COMPACT_POSTING_PTR(obj))->length);
    ASSERT_EQ(7, (COMPACT_POSTING_PTR(obj))->capacity);
    ASSERT_EQ(0, (COMPACT_POSTING_PTR(obj))->last_id());

    // upsert again
    posting_t::upsert(obj, 1002, {0, 3, 4});
    list = COMPACT_POSTING_PTR(obj);
    ASSERT_EQ(10, list->length);
    ASSERT_EQ(13, list->capacity);
    ASSERT_EQ(1002, list->last_id());

    free(list);
}

TEST(PostingListTest, DISABLED_Benchmark) {
    std::vector<uint32_t> offsets = {0, 1, 3};
    posting_list_t pl(4096);
    sorted_array arr;

    for(size_t i = 0; i < 500000; i++) {
        pl.upsert(i, offsets);
        arr.append(i);
    }

    auto begin = std::chrono::high_resolution_clock::now();

    for(size_t i = 250000; i < 250005; i++) {
        pl.upsert(i, offsets);
    }

    long long int timeMicros =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - begin).count();

    LOG(INFO) << "Time taken for 5 posting list updates: " << timeMicros;

    begin = std::chrono::high_resolution_clock::now();

    for(size_t i = 250000; i < 250005; i++) {
        arr.remove_value(i);
        arr.append(i);
    }

    timeMicros =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - begin).count();

    LOG(INFO) << "Time taken for 5 sorted array updates: " << timeMicros;
}

TEST(PostingListTest, DISABLED_BenchmarkIntersection) {
    std::vector<uint32_t> offsets = {0, 1, 3};

    time_t t;
    srand((unsigned) time(&t));

    std::set<uint32_t> rand_ids1;
    std::set<uint32_t> rand_ids2;
    std::set<uint32_t> rand_ids3;

    const size_t list1_size = 100000;
    const size_t list2_size = 50000;
    const size_t list3_size = 25000;
    const size_t num_range = 1000000;

    /*const size_t list1_size = 10;
    const size_t list2_size = 10;
    const size_t num_range = 50;*/

    for(size_t i = 0; i < list1_size; i++) {
        rand_ids1.insert(rand() % num_range);
    }

    for(size_t i = 0; i < list2_size; i++) {
        rand_ids2.insert(rand() % num_range);
    }

    for(size_t i = 0; i < list3_size; i++) {
        rand_ids3.insert(rand() % num_range);
    }

    posting_list_t pl1(1024);
    posting_list_t pl2(1024);
    posting_list_t pl3(1024);
    sorted_array arr1;
    sorted_array arr2;
    sorted_array arr3;

    std::string id1_str = "";
    std::string id2_str = "";
    std::string id3_str = "";

    for(auto id: rand_ids1) {
        //id1_str += std::to_string(id) + " ";
        pl1.upsert(id, offsets);
        arr1.append(id);
    }

    for(auto id: rand_ids2) {
        //id2_str += std::to_string(id) + " ";
        pl2.upsert(id, offsets);
        arr2.append(id);
    }

    for(auto id: rand_ids3) {
        //id2_str += std::to_string(id) + " ";
        pl3.upsert(id, offsets);
        arr3.append(id);
    }

    //LOG(INFO) << "id1_str: " << id1_str;
    //LOG(INFO) << "id2_str: " << id2_str;

    std::vector<uint32_t> result_ids;
    auto begin = std::chrono::high_resolution_clock::now();

    posting_list_t::intersect({&pl1, &pl2, &pl3}, result_ids);

    long long int timeMicros =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - begin).count();

    LOG(INFO) << "Posting list result len: " << result_ids.size();
    LOG(INFO) << "Time taken for posting list intersection: " << timeMicros;

    begin = std::chrono::high_resolution_clock::now();

    auto a = arr1.uncompress();
    auto b = arr2.uncompress();
    auto c = arr3.uncompress();

    uint32_t* ab;
    size_t ab_len = ArrayUtils::and_scalar(a, arr1.getLength(), b, arr2.getLength(), &ab);

    uint32_t* abc;
    size_t abc_len = ArrayUtils::and_scalar(ab, ab_len, c, arr3.getLength(), &abc);

    delete [] a;
    delete [] b;
    delete [] c;
    delete [] ab;
    delete [] abc;

    timeMicros =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - begin).count();

    LOG(INFO) << "Sorted array result len: " << abc_len;
    LOG(INFO) << "Time taken for sorted array intersection: " << timeMicros;
}
