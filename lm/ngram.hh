#ifndef LM_NGRAM_H__
#define LM_NGRAM_H__

#include "lm/base.hh"
#include "util/numbers.hh"
#include "util/string_piece.hh"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/unordered_map.hpp>

#include <vector>
#include <memory>

namespace lm {
namespace ngram {

namespace detail {
struct VocabularyFriend;
} // namespace detail

class Vocabulary : public base::Vocabulary {
	public:
	  Vocabulary() {}

		WordIndex Index(const std::string &str) const {
			return Index(StringPiece(str));
		}

		WordIndex Index(const StringPiece &str) const {
			boost::unordered_map<StringPiece, WordIndex>::const_iterator i(ids_.find(str));
			return (__builtin_expect(i == ids_.end(), 0)) ? not_found_ : i->second;
		}

		const char *Word(WordIndex index) const {
			return strings_[index].c_str();
		}

	protected:
		// friend interface for populating.
		friend struct detail::VocabularyFriend;

		WordIndex InsertUnique(std::auto_ptr<std::string> &word) {
			if (__builtin_expect(!ids_.insert(std::make_pair(StringPiece(*word), available_)).second, 0)) throw base::DuplicateWordLoadException(*word);
			strings_.push_back(word);
			return available_++;
		}

		void FinishedLoading() {
			SetSpecial(Index(StringPiece("<s>")), Index(StringPiece("</s>")), Index(StringPiece("<unk>")), available_);
		}

	private:
		// TODO: optimize memory use here by using one giant buffer, preferably premade by a binary file format.
		boost::ptr_vector<std::string> strings_;
		boost::unordered_map<StringPiece, WordIndex> ids_;
};

namespace detail {

struct IdentityHash : public std::unary_function<uint64_t, size_t> {
  size_t operator()(const uint64_t key) const { return key; }
};
struct Prob {
	Prob(float in_prob) : prob(in_prob) {}
	Prob() {}
  float prob;
};
struct ProbBackoff : Prob {
	ProbBackoff(float in_prob, float in_backoff) : Prob(in_prob), backoff(in_backoff) {}
	ProbBackoff() {}
	float backoff;
};

} // namespace detail

// Slow implementation just to verify Jon's description of SRI.
class Model {
	public:
		struct State {};

		Model(const char *arpa);

		const Vocabulary &GetVocabulary() const { return vocab_; }

		State BeginSentenceState() const {
			return State();
		}

    template <class ReverseHistoryIterator> LogDouble IncrementalScore(
        State &state,
        const ReverseHistoryIterator &hist_begin,
        const ReverseHistoryIterator &hist_end,
        const WordIndex new_word,
        unsigned int &ngram_length) const {
			uint32_t words[order_];
			words[0] = new_word;
			uint32_t *dest = &words[1];
			ReverseHistoryIterator src(hist_begin);
			for (; dest < words + order_; ++dest, ++src) {
				if (src == hist_end) {
					for (; dest < words + order_; ++dest) {
						*dest = vocab_.BeginSentence();
					}
					break;
				}
				*dest = *src;
			}
			// words is in reverse order.
			return InternalIncrementalScore(words, ngram_length);
		}

  private: 
		LogDouble InternalIncrementalScore(const uint32_t *words, unsigned int &ngram_length) const;

		size_t order_;

		Vocabulary vocab_;

		typedef boost::unordered_map<uint64_t, detail::Prob, detail::IdentityHash> Longest;
		Longest longest_;
		typedef boost::unordered_map<uint64_t, detail::ProbBackoff, detail::IdentityHash> Middle;
		std::vector<Middle> middle_vec_; 
		typedef std::vector<detail::ProbBackoff> Unigram;
		Unigram unigram_;
};

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM_H__