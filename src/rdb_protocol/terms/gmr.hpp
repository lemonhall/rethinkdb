#include "rdb_protocol/op.hpp"
#include "rdb_protocol/err.hpp"
#include "rdb_protocol/pb_utils.hpp"

namespace ql {

class gmr_term_t : public op_term_t {
public:
    gmr_term_t(env_t *env, const Term2 *term) : op_term_t(env, term, argspec_t(4)) { }
private:
    virtual val_t *eval_impl() {
        return new_val(arg(0)->as_seq()->gmr(arg(1)->as_func(),
                                             arg(2)->as_func(),
                                             arg(3)->as_func()));
    }
    RDB_NAME("grouped_map_reduce")
};

class rewrite_term_t : public term_t {
public:
    rewrite_term_t(env_t *env, const Term2 *term,
                   void (*rewrite)(env_t *, const Term2 *, Term2 *))
        : term_t(env), in(term) {
        rewrite(env, in, &out);
        //debugf("%s\n--->\n%s\n", in->DebugString().c_str(), out.DebugString().c_str());
        real.init(compile_term(env, &out));
    }
private:
    virtual val_t *eval_impl() { return real->eval(use_cached_val); }
    const Term2 *in;
    Term2 out;

    scoped_ptr_t<term_t> real;
};

class inner_join_term_t : public rewrite_term_t {
public:
    inner_join_term_t(env_t *env, const Term2 *term) :
        rewrite_term_t(env, term, rewrite) { }
    static void rewrite(env_t *env, const Term2 *in, Term2 *out) {
        rcheck(in->args_size() == 3, "Inner Join requires 3 arguments.");
        const Term2 *l = &in->args(0);
        const Term2 *r = &in->args(1);
        const Term2 *f = &in->args(2);
        int n = env->gensym();
        int m = env->gensym();

        Term2 *arg = out;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
        // `l`.concatmap { |n|
        N2(CONCATMAP, *arg = *l, arg = pb::set_func(arg, n);
           // `r`.concatmap { |m|
           N2(CONCATMAP, *arg = *r, arg = pb::set_func(arg, m);
              // r.branch(
              N3(BRANCH,
                 // r.funcall(`f`, n, m),
                 N3(FUNCALL, *arg = *f, pb::set_var(arg, n), pb::set_var(arg, m)),
                 // [{:left => n, :right => m}],
                 N1(MAKE_ARRAY,
                    OPT2(MAKE_OBJ,
                         "left", pb::set_var(arg, n),
                         "right", pb::set_var(arg, m))),
                 // [])}}
                 N0(MAKE_ARRAY))));
#pragma GCC diagnostic pop
    }
    RDB_NAME("inner_join")
};

class outer_join_term_t : public rewrite_term_t {
public:
    outer_join_term_t(env_t *env, const Term2 *term) :
        rewrite_term_t(env, term, rewrite) { }
    static void rewrite(env_t *env, const Term2 *in, Term2 *out) {
        rcheck(in->args_size() == 3, "Outer Join requires 3 arguments.");
        const Term2 *l = &in->args(0);
        const Term2 *r = &in->args(1);
        const Term2 *f = &in->args(2);
        int n = env->gensym();
        int m = env->gensym();
        int lst = env->gensym();

        Term2 *arg = out;
        // TODO: implement a new primitive to do this without coercing

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
        // `l`.concatmap { |n|
        N2(CONCATMAP, *arg = *l, arg = pb::set_func(arg, n);
           // r.funcall(lambda { |lst
           N2(FUNCALL, arg = pb::set_func(arg, lst);
              // r.branch(
              N3(BRANCH,
                 // r.gt(r.count(lst), 0),
                 N2(GT, N1(COUNT, pb::set_var(arg, lst)), pb::set_int(arg, 0)),
                 // lst,
                 pb::set_var(arg, lst),
                 // [{:left => n}])},
                 N1(MAKE_ARRAY, OPT1(MAKE_OBJ, "left", pb::set_var(arg, n)))),
              // r.coerce(
              N2(COERCE,
                 // `r`.concatmap { |m|
                 N2(CONCATMAP, *arg = *r, arg = pb::set_func(arg, m);
                    // r.branch(
                    N3(BRANCH,
                       // r.funcall(`f`, n, m),
                       N3(FUNCALL, *arg = *f, pb::set_var(arg, n), pb::set_var(arg, m)),
                       // [{:left => n, :right => m}],
                       N1(MAKE_ARRAY,
                          OPT2(MAKE_OBJ,
                               "left", pb::set_var(arg, n),
                               "right", pb::set_var(arg, m))),
                       // [])},
                       N0(MAKE_ARRAY))),
                 // "ARRAY"))}
                 pb::set_str(arg, "ARRAY"))))
#pragma GCC diagnostic pop
    }
    RDB_NAME("outer_join")
};

} // namespace ql
