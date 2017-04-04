/****************************************************************************************[Solver.h]
 The MIT License (MIT)

 Copyright (c) 2014, Sam Bayless

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **************************************************************************************************/
#include "monosat/mtl/Vec.h"
#include "monosat/graph/ReachDetector.h"
#include "monosat/dgl/RamalReps.h"
#include "monosat/dgl/BFS.h"
#include "monosat/graph/GraphTheory.h"
#include "monosat/core/Config.h"
#include "monosat/dgl/DynamicConnectivity.h"
#include "monosat/dgl/TarjansSCC.h"
#include "monosat/graph/GraphHeuristic.h"
#include "monosat/graph/MaxflowDetector.h"
#include "monosat/dgl/CachedReach.h"
using namespace Monosat;
template<typename Weight>
ReachDetector<Weight>::ReachDetector(int _detectorID, GraphTheorySolver<Weight> * _outer, DynamicGraph<Weight>  &_g,
		DynamicGraph<Weight>  &_antig, int from, double seed) :
		Detector(_detectorID), outer(_outer), g_under(_g), g_over(_antig), within(-1), source(from), rnd_seed(seed) { //,chokepoint_status(*this),chokepoint(chokepoint_status, _antig,source){

	//opt_path=nullptr;
	chokepoint_detector = nullptr;
	cutgraph_detector = nullptr;
	positiveReachStatus = nullptr;
	negativeReachStatus = nullptr;
	underapprox_detector = nullptr;
	overapprox_reach_detector = nullptr;
	underapprox_path_detector = nullptr;
	overapprox_path_detector = nullptr;
	first_reach_var = var_Undef;
	stats_pure_skipped = 0;
	stats_shrink_removed = 0;
	underprop_marker = CRef_Undef;
	overprop_marker = CRef_Undef;
	forced_edge_marker = CRef_Undef;
	if (reachalg == ReachAlg::ALG_SAT) {
		//to print out the solution
		//positive_reach_detector = new ReachDetector::CNFReachability(*this,false);
		//negative_reach_detector = new ReachDetector::CNFReachability(*this,true);

		underapprox_path_detector = new UnweightedBFS<Weight,Distance<int>::NullStatus>(from, _g, Distance<int>::nullStatus, 1);
		overapprox_path_detector = new UnweightedBFS<Weight,Distance<int>::NullStatus>(from, _antig, Distance<int>::nullStatus, -1);
		underapprox_fast_detector = underapprox_path_detector;
		return;
	}

	if (opt_decide_graph_chokepoints) {
		chokepoint_detector = new DFSReachability<Weight, Reach::NullStatus>(from, _antig, Reach::nullStatus, 1);

	}
	if (opt_shrink_theory_conflicts) {
		cutgraph_detector = new UnweightedRamalReps<Weight,Reach::NullStatus>(from, cutgraph, Reach::nullStatus, 0);
	}



	/* if(opt_use_optimal_path_for_decisions){
	 opt_path = new WeightedDijkstra< OptimalWeightEdgeStatus >(from,_antig,opt_weight);
	 }*/
	positiveReachStatus = new ReachDetector<Weight>::ReachStatus(*this, true);
	negativeReachStatus = new ReachDetector<Weight>::ReachStatus(*this, false);
	if (reachalg == ReachAlg::ALG_BFS) {
		if (!opt_encode_reach_underapprox_as_sat) {
			underapprox_detector = new BFSReachability<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*(positiveReachStatus), 1);
		} else {
			underapprox_fast_detector = new BFSReachability<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*(positiveReachStatus), 1);
			//positive_reach_detector = new ReachDetector::CNFReachability(*this,false);
		}

		overapprox_reach_detector = new BFSReachability<Weight,ReachDetector<Weight>::ReachStatus>(from, _antig,
				*(negativeReachStatus), -1);

		underapprox_path_detector = underapprox_detector;
		overapprox_path_detector = overapprox_reach_detector;
		negative_distance_detector = (Distance<int> *) overapprox_path_detector;
	} else if (reachalg == ReachAlg::ALG_DFS) {
		if (!opt_encode_reach_underapprox_as_sat) {
			underapprox_detector = new DFSReachability<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*(positiveReachStatus), 1);
		} else {
			underapprox_fast_detector = new DFSReachability<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*(positiveReachStatus), 1);
			//positive_reach_detector = new ReachDetector::CNFReachability(*this,false);
		}

		overapprox_reach_detector = new DFSReachability<Weight,ReachDetector<Weight>::ReachStatus>(from, _antig,
				*(negativeReachStatus), -1);
		if (opt_conflict_shortest_path)
			underapprox_path_detector = new UnweightedBFS<Weight,Distance<int>::NullStatus>(from, _g, Distance<int>::nullStatus, 1);
		else
			underapprox_path_detector = underapprox_detector;

		negative_distance_detector = new UnweightedBFS<Weight,Distance<int>::NullStatus>(from, _antig, Distance<int>::nullStatus, -1);
		overapprox_path_detector = overapprox_reach_detector;
	} else if (reachalg == ReachAlg::ALG_DISTANCE) {
		if (!opt_encode_reach_underapprox_as_sat) {
			underapprox_detector = new UnweightedBFS<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*(positiveReachStatus), 1);
		} else {
			underapprox_fast_detector = new UnweightedBFS<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*(positiveReachStatus), 1);
			//positive_reach_detector = new ReachDetector::CNFReachability(*this,false);
		}

		overapprox_reach_detector = new UnweightedBFS<Weight,ReachDetector<Weight>::ReachStatus>(from, _antig,
				*(negativeReachStatus), -1);
		underapprox_path_detector = underapprox_detector;
		overapprox_path_detector = overapprox_reach_detector;
		negative_distance_detector = (Distance<int> *) overapprox_path_detector;
	} else if (reachalg == ReachAlg::ALG_RAMAL_REPS) {
		if (!opt_encode_reach_underapprox_as_sat) {
			underapprox_detector = new UnweightedRamalReps<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*(positiveReachStatus), 1, false);
			underapprox_path_detector=underapprox_detector;
		} else {
			underapprox_fast_detector = new UnweightedRamalReps<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*(positiveReachStatus), 1, false);
			underapprox_path_detector=underapprox_fast_detector;
			//positive_reach_detector = new ReachDetector::CNFReachability(*this,false);
		}

		overapprox_reach_detector = new UnweightedRamalReps<Weight,ReachDetector<Weight>::ReachStatus>(from, _antig,
				*(negativeReachStatus), -1, false);

		overapprox_path_detector=overapprox_reach_detector;
		//RamalReps now supports finding paths
		//underapprox_path_detector = new UnweightedBFS<Weight,Distance<int>::NullStatus>(from, _g, Distance<int>::nullStatus, 1);
		//overapprox_path_detector = new UnweightedBFS<Weight,Distance<int>::NullStatus>(from, _antig, Distance<int>::nullStatus, -1);
		negative_distance_detector = (Distance<int> *) overapprox_path_detector;
	}/*else if (reachalg==ReachAlg::ALG_THORUP){


	 positive_reach_detector = new DynamicConnectivity<ReachDetector<Weight>::ReachStatus>(_g,*(positiveReachStatus),1);
	 negative_reach_detector = new DynamicConnectivity<ReachDetector<Weight>::ReachStatus>(_antig,*(negativeReachStatus),-1);
	 positive_path_detector = positive_reach_detector;
	 if(opt_conflict_shortest_path)
	 positive_path_detector = new Distance<NullEdgeStatus>(from,_g,nullEdgeStatus,1);
	 else
	 positive_path_detector =positive_reach_detector;
	 }*/else {
		if (!opt_encode_reach_underapprox_as_sat) {
			underapprox_detector = new UnweightedDijkstra<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*positiveReachStatus, 1);
		} else {
			underapprox_fast_detector = new UnweightedDijkstra<Weight,ReachDetector<Weight>::ReachStatus>(from, _g,
					*positiveReachStatus, 1);
			//positive_reach_detector = new ReachDetector::CNFReachability(*this,false);
		}
		overapprox_reach_detector = new UnweightedDijkstra<Weight,ReachDetector<Weight>::ReachStatus>(from, _antig,
				*negativeReachStatus, -1);
		underapprox_path_detector = underapprox_detector;
		overapprox_path_detector = overapprox_reach_detector;
		negative_distance_detector = (Distance<int> *) overapprox_path_detector;
		//reach_detectors.last()->positive_dist_detector = new Dijkstra(from,g);
	}
	if (underapprox_detector && !underapprox_fast_detector)
		underapprox_fast_detector = underapprox_detector;

	if (opt_reach_detector_combined_maxflow) {
		if (mincutalg == MinCutAlg::ALG_EDKARP_DYN) {
			conflict_flow = new EdmondsKarpDynamic<int64_t>(outer->cutGraph,  source, 0);
		} else if (mincutalg == MinCutAlg::ALG_EDKARP_ADJ) {
			conflict_flow = new EdmondsKarpAdj<int64_t>(outer->cutGraph,  source, 0);
		} else if (mincutalg == MinCutAlg::ALG_DINITZ) {
			conflict_flow = new Dinitz<int64_t>(outer->cutGraph,  source, 0);
		} else if (mincutalg == MinCutAlg::ALG_DINITZ_LINKCUT) {
			//link-cut tree currently only supports ints (enforcing this using tempalte specialization...).

			conflict_flow = new Dinitz<int64_t>(outer->cutGraph,  source, 0);

		} else if (mincutalg == MinCutAlg::ALG_KOHLI_TORR) {
			if (opt_use_kt_for_conflicts) {
				conflict_flow = new KohliTorr<int64_t>(outer->cutGraph,  source, 0,
						opt_kt_preserve_order);
			} else
				conflict_flow = new EdmondsKarpDynamic<int64_t>(outer->cutGraph,  source, 0);
		} else {
			conflict_flow = new EdmondsKarpAdj<int64_t>(outer->cutGraph,  source, 0);
		}
	}

	if(opt_graph_cache_propagation){
		if(underapprox_detector){
			Reach* original_underapprox_detector = underapprox_detector;
			underapprox_detector = new CachedReach<Weight,ReachDetector<Weight>::ReachStatus>(original_underapprox_detector, _g,*(positiveReachStatus),1);
            if(opt_graph_use_cache_for_decisions>=1) {
                if (underapprox_path_detector == original_underapprox_detector) {
                    underapprox_path_detector = underapprox_detector;
                } else {
                    underapprox_path_detector = new CachedReach<Weight>(underapprox_path_detector, _g);
                }
            }
		}
		if(overapprox_reach_detector){
			Reach* original_overapprox_detector = overapprox_reach_detector;
			overapprox_reach_detector = new CachedReach<Weight,ReachDetector<Weight>::ReachStatus>(original_overapprox_detector, _antig,*(negativeReachStatus),-1);
            if(opt_graph_use_cache_for_decisions>=1) {
                if (overapprox_path_detector == original_overapprox_detector) {
                    overapprox_path_detector = overapprox_reach_detector;
                } else {
                    overapprox_path_detector = new CachedReach<Weight>(overapprox_path_detector, _antig);
                }
            }
		}
	}

	if (underapprox_detector)
		underapprox_detector->setSource(source);
	if (overapprox_reach_detector)
		overapprox_reach_detector->setSource(source);

	underprop_marker = outer->newReasonMarker(getID());
	overprop_marker = outer->newReasonMarker(getID());
	forced_edge_marker = outer->newReasonMarker(getID());



}
template<typename Weight>
void ReachDetector<Weight>::buildSATConstraints(bool onlyUnderApprox, int within_steps) {
	if (within_steps < 0)
		within_steps = g_under.nodes();
	if (within_steps > g_under.nodes())
		within_steps = g_under.nodes();
	if (within_steps > g_under.edges())
		within_steps = g_under.edges();
	if (onlyUnderApprox && constraintsBuiltUnder >= within_steps)
		return;
	if (!onlyUnderApprox && constraintsBuiltOver >= within_steps)
		return;

	//there is no reason to encode these variables in the theory solver!

	if (!onlyUnderApprox) {
		assert(outer->decisionLevel() == 0);
		vec<Lit> c;

		if (constraintsBuiltOver <= 0) {
			constraintsBuiltOver = 0;
			dist_lits.push();
			Lit True = mkLit(outer->newVar());
			outer->addClause(True);

			Lit False = ~True;

			for (int i = 0; i < g_under.nodes(); i++) {
				dist_lits[0].push(False);
			}
			dist_lits[0][source] = True;
		}

		vec<Lit> reaches;

		vec<Lit> incomingEdges;
		vec<Lit> incomingNodes;

		//Note (thanks to Roberto Sebastiani for this): clauses added here do not count towards pure literal counts.

		//bellman-ford:
		for (int i = constraintsBuiltOver; i < within_steps; i++) {
			dist_lits.last().copyTo(reaches);
			dist_lits.push();
			reaches.copyTo(dist_lits.last());

			//For each edge:
			for (int j = 0; j < g_under.nodes(); j++) {
				Lit r_cur = reaches[j];

				for (Edge & e : outer->inv_adj[j]) {
					//Edge e = outer->edges[j][k];
					assert(e.to == j);
					if (outer->value(dist_lits.last()[e.to]) == l_True) {
						//do nothing
					} else if (outer->value(reaches[e.from]) == l_False) {
						//do nothing
					} else {
						Lit l = mkLit(e.v, false);

						Lit r = mkLit(outer->newVar(), false);

						c.clear();
						c.push(~r);
						c.push(r_cur);
						c.push(l); //r -> (e.l or reaches[e.to])
						outer->addClause(c);
						c.clear();
						c.push(~r);
						c.push(r_cur);
						c.push(reaches[e.from]); //r -> (reaches[e.from]) or reaches[e.to])
						outer->addClause(c);
						c.clear();
						c.push(r);
						c.push(~r_cur); //~r -> ~reaches[e.to]
						outer->addClause(c);
						c.clear();
						c.push(r);
						c.push(~reaches[e.from]);
						c.push(~l); //~r -> (~reaches[e.from] or ~e.l)
						outer->addClause(c);
						r_cur = r;

					}
				}
				dist_lits.last()[j] = r_cur;
			}

		}
		assert(dist_lits.size() == within_steps + 1);
		if (within_steps == g_under.nodes() || within_steps == g_under.edges()) {
			reach_lits.growTo(g_under.nodes());

			for (int i = 0; i < dist_lits.last().size(); i++) {
				Lit d = dist_lits.last()[i];
				if (reach_lits[i] == lit_Undef) {
					reach_lits[i] = d;
				} else {
					outer->makeEqual(reach_lits[i], d);
				}
			}

		}

		constraintsBuiltOver = within_steps;
	} else {
		if (constraintsBuiltUnder < 0) {
			constraintsBuiltUnder = g_under.nodes();
			cnf_reach_lits.growTo(g_under.nodes(),lit_Undef);

			//for each node, it cannot be reachable if none of its incoming.edges() are enabled.
			for (int n = 0; n < g_under.nodes(); n++) {
				if (reach_lits[n]!=lit_Undef){
					cnf_reach_lits[n]=reach_lits[n];
				}else if (cnf_reach_lits[n] == lit_Undef) {
					Var reach_var;
					//if (this->underapprox_detector || this->overapprox_reach_detector)
					//	reach_var = outer->newVar(detectorID, true);
					//else {
						reach_var = outer->newVar(-1, false);
					//}
					cnf_reach_lits[n] = mkLit(reach_var); //since this is _only_ the underapproximation, these variables _do_ need to be connected to the theory solver


				}
			}

			vec<Lit> c;
			vec<Lit> t;
			for (int n = 0; n < g_under.nodes(); n++) {

				if (n == source) {
					outer->addClause(cnf_reach_lits[n]); //source node is unconditionally reachable
				} else {
					c.clear();
					c.push(~cnf_reach_lits[n]);
					//for(auto edge:g.inverted_adjacency[n]){
					for (int i = 0; i < g_under.nIncoming(n); i++) {
						auto & edge = g_under.incoming(n, i);
						int edgeID = edge.id;
						int from = edge.node;
						if (from != n) {
							//ignore trivial cycles
							Var v = outer->getEdgeVar(edgeID);
							c.push(mkLit(v));
						}
					}

					//Either an edge must be true, or the reach_lit must be false (unless this is the source reach node);
					outer->addClause(c);
					c.clear();
					//either an incoming node must be true, or reach_lit must be false
					c.push(~cnf_reach_lits[n]);
					for (int i = 0; i < g_under.nIncoming(n); i++) {
						auto & edge = g_under.incoming(n, i);
						int edgeID = edge.id;
						int from = edge.node;
						if (from != n) { //ignore trivial cycles
							c.push(cnf_reach_lits[from]);
						}
					}
					//Either at least one incoming node must be true, or the reach_lit must be false (unless this is the source reach node);
					outer->addClause(c);

					//Either at least incoming edge AND its corresponding from node must BOTH be simultaneously true, or the node is must not be reachable
					c.clear();
					//either an incoming node must be true, or reach_lit must be false
					c.push(~cnf_reach_lits[n]);
					for (int i = 0; i < g_under.nIncoming(n); i++) {
						auto & edge = g_under.incoming(n, i);
						int edgeID = edge.id;
						int from = edge.node;
						if (from != n) { //ignore trivial cycles
							Lit e = mkLit(outer->getEdgeVar(edgeID));
							Lit incoming = cnf_reach_lits[from];

							Lit andGate = mkLit(outer->newVar());
							//Either the andgate is true, or at least one of e, incoming is false
							outer->addClause(andGate, ~incoming, ~e);
							//If andgate is true, incoming is true
							outer->addClause(~andGate, incoming);
							outer->addClause(~andGate, e);
							c.push(andGate);
						}
					}
					//Either one andGate must be true, or the reach_lit must be false
					outer->addClause(c);
				}

				//If this node is reachable, the for each outgoing edge, if that edge is enabled, its to node must also be reachable
				for (int i = 0; i < g_under.nIncident(n); i++) {
					auto & edge = g_under.incident(n, i);
					int edgeID = edge.id;
					int to = edge.node;
					if (to != n) { //ignore trivial cycles
						Lit e = mkLit(outer->getEdgeVar(edgeID));
						Lit outgoing = cnf_reach_lits[to];
						outer->addClause(~cnf_reach_lits[n], ~e, outgoing);
					}
				}

			}
		}
	}

}

template<typename Weight>
class ReachHeuristic : public GraphHeuristic<Weight>{

    ReachDetector<Weight> * r;

    LSet to_decide;

    WeightedDijkstra<Weight,double> * rnd_path = nullptr;
    std::vector<double> rnd_weight;
    GraphTheorySolver<Weight> *outer;
    Lit reach_lit = lit_Undef;
	int last_over_modification=-1;
	int last_over_addition=-1;
	int last_over_deletion=-1;
	int over_history_qhead=0;
	int last_over_history_clear=0;

    int last_under_history_clear=0;
    int under_history_qhead=0;
    int last_under_modification=-1;
    int last_under_addition=-1;
    int last_under_deletion=-1;

	DynamicGraph<Weight> & g_over;
    DynamicGraph<Weight> & g_under;
	IntSet<int> path_edges;
	bool path_is_cut=false;
    int dest_node=-1;
    Heuristic * sub_heuristic=nullptr;
public:
    ReachHeuristic(GraphTheorySolver<Weight> * outer, ReachDetector<Weight> * r, Lit reach_lit,int dest_node): GraphHeuristic<Weight>(outer,r), r(r),outer(outer),reach_lit(reach_lit),g_over(r->g_over),g_under(r->g_under),dest_node(dest_node) {
		this->setPriority(outer->getSolver()->getDecisionPriority(var(outer->toSolver(reach_lit))));
		if (opt_use_random_path_for_decisions) {
            rnd_weight.clear();
            rnd_path = new WeightedDijkstra<Weight,double>(r->source, r->g_over, rnd_weight);
            for (int i = 0; i < outer->edge_list.size(); i++) {
                double w = drand(r->rnd_seed);

                rnd_weight.push_back(w);
            }

        }

    }

    void setSubHeuristic(Heuristic* h){
        sub_heuristic=h;
    }

	void computePath(){
		path_is_cut=false;
		auto * over_reach = r->overapprox_reach_detector;
		auto * under_reach = r->underapprox_detector;

		if (!under_reach) {
			under_reach = r->underapprox_fast_detector;
		}

		static int iter = 0;
		if(++iter==109){
			int a=1;
		};

		auto * over_path = r->overapprox_path_detector;
		auto * under_path = r->underapprox_detector;
		if (!under_path) {
			under_path = r->underapprox_path_detector;
		}
		assert(under_path);
		assert(over_path);
		assert(over_reach);
		assert(under_reach);
        if(opt_graph_use_cache_for_decisions==1){
            over_path->clearCache();
        }

		to_decide.clear();
		path_edges.clear();

		{
			Lit l = reach_lit;
			assert (l != lit_Undef);

			int j = r->getNode(var(l));
			if (outer->value(l) == l_True && opt_decide_graph_pos) {
				//if(S->level(var(l))>0)
				//	continue;

				if (over_path->connected(j)) { // && !under_reach->connected(j) //this check is not needed
					//then lets try to connect this

					to_decide.clear();


					assert(over_path->connected(j));			//Else, we would already be in conflict
					int p = j;
					int last_edge = -1;
					int last = j;
					if (!opt_use_random_path_for_decisions) {
						/*if(opt_use_optimal_path_for_decisions){
						 //ok, read back the path from the over to find a candidate edge we can decide
						 //find the earliest unconnected node on this path
						 opt_path->update();
						 p = j;
						 last = j;
						 while(!under->connected(p)){

						 last=p;
						 assert(p!=source);
						 last_edge=opt_path->incomingEdge(p);
						 int prev = opt_path->previous(p);
						 p = prev;

						 }
						 }else*/{
							//ok, read back the path from the over to find a candidate edge we can decide
							//find the earliest unconnected node on this path
							over_path->update();

							p = j;
							last = j;
							//while (!under_reach->connected(p)) {
                            while(p!=r->source){
								last = p;
								assert(p != r->source);
								last_edge = over_path->incomingEdge(p);
                                path_edges.insert(last_edge);
								Var edge_var = outer->getEdgeVar(last_edge);
								if (outer->value(edge_var) == l_Undef) {

									to_decide.push(mkLit(edge_var, false));
								}
								int prev = over_path->previous(p);
								p = prev;

							}
						}
					} else {
						//Randomly re-weight the graph sometimes
						if (drand(r->rnd_seed) < opt_decide_graph_re_rnd) {

							for (int i = 0; i < outer->edge_list.size(); i++) {
								double w = drand(r->rnd_seed);
								/* w-=0.5;
								 w*=w;*/
								//printf("%f (%f),",w,rnd_seed);
								//rnd_path->setWeight(i,w);
								rnd_weight[i] = w;
							}
						}
						rnd_path->update();
						//derive a random path in the graph
						p = j;
						last = j;
						assert(rnd_path->connected(p));
						while (!under_reach->connected(p)) {

							last = p;
							assert(p !=  r->source);
							last_edge = rnd_path->incomingEdge(p);
                            path_edges.insert(last_edge);
							Var edge_var = outer->getEdgeVar(last_edge);
							if (outer->value(edge_var) == l_Undef) {

								to_decide.push(mkLit(edge_var, false));
							}
							int prev = rnd_path->previous(p);
							p = prev;
							assert(p >= 0);
						}

					}


				}
			} else if (outer->value(l) == l_False && opt_decide_graph_neg) {

				//for each negated reachability constraint, we can find a cut through the unassigned edges in the over-approx and disable one of those edges.
				assert(!under_path->connected(j));
				over_path->update();
				if (over_reach->connected(j) && !under_reach->connected(j)) {
					//then lets try to disconnect this node from source by walking back along the path in the over approx, and disabling the first unassigned edge we see.
					//(there must be at least one such edge, else the variable would be connected in the under approximation as well - in which case it would already have been propagated.
					path_is_cut=true;
					to_decide.clear();


                    //FIXME: The below walks back along the path to find unassigned edges, when it should instead be finding a cut of unassigned edges.

					int p = j;
					int last = j;
					while (!under_reach->connected(p)) {
						last = p;
						assert(p !=  r->source);
						int prev = over_path->previous(p);
						int incoming_edge = over_path->incomingEdge(p);

						Var v = outer->edge_list[incoming_edge].v;
						if (outer->value(v) == l_Undef) {
                            path_edges.insert(incoming_edge);
							to_decide.push(mkLit(v, true));
						} else {
							assert(outer->value(v)!=l_False);
						}
						p = prev;
					}


				}

			}

		}

	}

	bool needsRecompute(){
		if (outer->value(reach_lit) != l_False && path_is_cut) {
			return true;
		}else if (outer->value(reach_lit) != l_True && !path_is_cut) {
			return true;
		}


		//check if any edges on the path have been removed since the last update
		if (last_over_modification > 0 && g_over.modifications == last_over_modification && last_under_modification > 0 && g_under.modifications == last_under_modification){
			return false;
		}
		if (last_over_modification <= 0 || g_over.changed() || last_under_modification <= 0 || g_under.changed()) {//Note for the future: there is probably room to improve this further.
			return true;
		}

		if(!path_edges.size()){
			return true;
		}

		if (last_over_history_clear != g_over.historyclears || last_under_history_clear != g_under.historyclears) {
			over_history_qhead = g_over.historySize();
			last_over_history_clear = g_over.historyclears;
            under_history_qhead = g_under.historySize();
            last_under_history_clear = g_under.historyclears;
            to_decide.clear();
			for(int edgeID:path_edges){
				if(!path_is_cut) {
					if (!g_over.edgeEnabled(edgeID))
						return true;
                    Var edge_var = outer->getEdgeVar(edgeID);
                    Lit l = mkLit(edge_var, false);

                    if (outer->value(edge_var) == l_Undef) {

                        to_decide.push(l);
                    }
				}else{
					if (g_under.edgeEnabled(edgeID))
						return true;
                    Var edge_var = outer->getEdgeVar(edgeID);
                    Lit l = mkLit(edge_var, true);
                    if (outer->value(edge_var) == l_Undef) {

                        to_decide.push(l);
                    }
				}
			}
		}

		for (int i = over_history_qhead; i < g_over.historySize(); i++) {
			int edgeid = g_over.getChange(i).id;

			if (g_over.getChange(i).addition && g_over.edgeEnabled(edgeid)) {
                if(!path_is_cut) {

                }else{
                    Var edge_var = outer->getEdgeVar(edgeid);
                    Lit l = mkLit(edge_var, true);
                    if (outer->value(edge_var) == l_Undef) {

                        to_decide.push(mkLit(edge_var, true));
                    }
                }
			} else if (!g_over.getChange(i).addition && !g_over.edgeEnabled(edgeid)) {
				if(!path_is_cut) {
					if (path_edges.has(edgeid)) {
						return true;
					}
				}else{

                }
			}
		}

        for (int i = under_history_qhead; i < g_under.historySize(); i++) {
            int edgeid = g_under.getChange(i).id;

            if (path_edges.has(edgeid)) {
                if (!g_under.getChange(i).addition && !g_under.edgeEnabled(edgeid)) {
                    if (!path_is_cut) {
                        Var edge_var = outer->getEdgeVar(edgeid);
                        Lit l = mkLit(edge_var, false);
                        if (outer->value(edge_var) == l_Undef) {

                            to_decide.push(mkLit(edge_var, false));
                        }
                    }else {

                    }
                } else if (g_under.getChange(i).addition && g_under.edgeEnabled(edgeid)) {
                    if (!path_is_cut) {

                    }else {
                        if (path_edges.has(edgeid)) {
                            return true;
                        }
                    }
                }
            }
        }

        last_over_modification = g_over.modifications;
        last_over_deletion = g_over.deletions;
        last_over_addition = g_over.additions;
        over_history_qhead = g_over.historySize();
        last_over_history_clear = g_over.historyclears;

        last_under_modification = g_under.modifications;
        last_under_deletion = g_under.deletions;
        last_under_addition = g_under.additions;
        under_history_qhead = g_under.historySize();
        last_under_history_clear = g_under.historyclears;
		return false;
	}

    void drawGrid(DynamicGraph<Weight> & g, bool only_path=false){
        int width =10;
        int height = 10;



		vec<int> startsX;
		vec<int> endsX;
		vec<int> startsY;
		vec<int> endsY;

		/*IntSet<int> startsX;
		IntSet<int> endsX;
		IntSet<int> startsY;
		IntSet<int> endsY;*/
		vec<std::pair<int,int>> starts;
		vec<std::pair<int,int>> ends;
		/*for(int i = 0;i<80;i+=4){
			int sY = sourcenodes[i];
			int sX = sourcenodes[i+1];
			int eY = sourcenodes[i+2];
			int eX = sourcenodes[i+3];
			startsX.push(sX);
			startsY.push(sY);
			endsX.push(eX);
			endsY.push(eY);
			starts.push();
			starts.last().first = sX;
			starts.last().second = sY;
			ends.push();
			ends.last().first = eX;
			ends.last().second = eY;
		}*/

        bool found_source = false;
        bool found_dest = false;
        printf("\n");
        for(int y = 0;y<height;y++) {
            for (int x = 0; x < width; x++){
                int f_node = (y*height + x)*2+1;
				std::pair <int, int> p = std::make_pair(x,y);
                if (f_node == r->source){
					//assert(starts.contains(p));
                    found_source=true;
                    printf("*");
                }else if (f_node == dest_node){
                    found_dest=true;
					//assert(ends.contains(p));
                    printf("@");
                }else{

					if(starts.contains(p)){
						int id = starts.indexOf(p)%10;
						printf("%d",id);
					}else if(ends.contains(p)){
						//printf("%%");
						int id = ends.indexOf(p)%10;
						printf("%d",id);
					}else {
						printf("+");
					}
				}
                if (x<width-1) {
                    int t_node = (y*height+x+1)*2;
                    //assert(g.hasEdge(f_node,t_node));
                    if(g.hasEdge(f_node, t_node) || g.hasEdge(t_node, f_node)){
                        if(only_path){
                            int edgea =    g.getEdge(f_node, t_node);
                            int edgeb =    g.getEdge(t_node, f_node);
                            if(path_edges.has(edgea) || path_edges.has(edgeb)){
                                printf("-");
                            }else{
                                printf(" ");
                            }
                        }else {
                            //int to_edge = g.getEdge(f_node,t_node);
                            //if(g.edgeEnabled(to_edge)){
                            printf("-");
                        }
                    }else{
                        printf(" ");
                    }
                }
            }
            printf("\n");
            for (int x = 0; x < width; x++) {
                int f_node = y*height + x;
                if (y < height - 1) {
                    int t_node = (y + 1) * height + x;
                    //assert(g.hasEdge(f_node, t_node));
					if(only_path) {
						int edgea = g.getEdge(f_node, t_node);
						int edgeb = g.getEdge(t_node, f_node);

						if (path_edges.has(edgea)) {
							assert(g.hasEdge(edgea));
							printf("| ");
						}else if  (path_edges.has(edgeb)){
							assert(g.hasEdge(edgeb));
							printf("| ");
						} else {
							printf("  ");
						}
					}else if(g.hasEdge(f_node, t_node) || g.hasEdge(t_node, f_node)){
                    //int to_edge = g.getEdge(f_node, t_node);
                    //if(g.edgeEnabled(to_edge)){
                            //int to_edge = g.getEdge(f_node,t_node);
                            //if(g.edgeEnabled(to_edge)){
                            printf("| ");
                    }else{
                        printf("  ");
                    }
                }
            }
            printf("\n");
        }
        printf("\n");
        assert(found_source);
        assert(found_dest);
    }

    Lit decide(CRef &decision_reason)override{

        if(sub_heuristic){
            Lit l = sub_heuristic->decideTheory(decision_reason);
            if (l!=lit_Undef)
                return l;
        }

		if (outer->value(reach_lit)==l_Undef){
			return lit_Undef;//if the reach lit is unassigned, do not make any decisions here
		}
        static int iter = 0;
        if(++iter==226){
            int a=1;
        }

        {
            //Routing ideas from Alex Nadel's FMCAD16 paper
            //1) After a path is established, consider deciding remaining edges to false (only appropriate for some problem domains)
            //2) When searching for a shortest path, consider setting the weight of already decided edges to 0, to encourage re-use
            //3) The edge encoding in Alex's paper would be similar to an explicit 'at-most-one-graph-per-edge' framework,
            //   allowing some learned clauses to be more more efficiently encoded (?).
            //4)Net swapping. When a reach constraint E is violated, it may be because of decisions made to connect another reach constraint (B).
            // If we have reach constraints {A,B,C,D,E}, decided in that order, and D is in conflict because of edge assignments made for B,
            // then net swapping will change the reach decision order as follows: {A,D,B,C,E}. Questions: How does this compare to VSIDS approach,
            // and how is the conflicting reach constraint (B) determined?
            // "A reach constraint B blocks reach constraint D, if any edge on the separating cut that is blocking D, is an edge
            // connecting reach constraint B." Is it the case that at most one reach constraint can be a blocking reach constraint?
            // Unclear, if more than just two reach constraints are considered.
            //5) Net restarting. Associate a conflict ocunter with each reach constraint. When it reaches a threshold T ( 10 by default),
            // restart and place that reach constraint at the front of the reach decision order (and set all conflict ocunters to 0). This
            // is similar to the current vsids approach.

            //Things from FMCAD16 that do not need to be done: Early net conflict detection - that is already handled by SMMT
            //

        }

		if(needsRecompute()){
            r->stats_heuristic_recomputes++;
			computePath();
           /* if(opt_verb>=4) {
                printf("Over:\n");
                drawGrid(g_over);
                printf("Under:\n");
                drawGrid(g_under);
                printf("Path:\n");
                drawGrid(g_over, true);
            }*/
			last_over_modification = g_over.modifications;
			last_over_deletion = g_over.deletions;
			last_over_addition = g_over.additions;
			over_history_qhead = g_over.historySize();
			last_over_history_clear = g_over.historyclears;

            last_under_modification = g_under.modifications;
            last_under_deletion = g_under.deletions;
            last_under_addition = g_under.additions;
            under_history_qhead = g_under.historySize();
            last_under_history_clear = g_under.historyclears;

        }
#ifdef DEBUG_GRAPH
		for(Lit l:to_decide){
			assert(outer->value(l)!=l_False);
		}

		if(!path_is_cut){
			for(int edgeID:path_edges){
				assert(g_over.edgeEnabled(edgeID));
				Lit l = mkLit(outer->getEdgeVar(edgeID));
                if(outer->value(l)==l_Undef){
                    assert(to_decide.contains(l));
                }
			}

		}else{
			for(int edgeID:path_edges){
				assert(!g_under.edgeEnabled(edgeID));
                Lit l = mkLit(outer->getEdgeVar(edgeID));
                if(outer->value(l)==l_Undef){
                    assert(to_decide.contains(~l));
                }
			}
		}
#endif

		if (to_decide.size()) {//  && last_decision_status == over_path->numUpdates() the numUpdates() monitoring strategy doesn't work if the constraints always force edges to be assigned false when other edges
            // are assigned true, as the over approx graph will always register as having been updated.
            //instead, check the history of the dynamic graph to see if any of the edges on the path have been assigned false, and only recompute in that case.
            while (to_decide.size()) {
                Lit l = to_decide.last();

                if (outer->value(l) == l_Undef) {
                    //stats_decide_time += rtime(2) - startdecidetime;
                    return l;
                }else if(outer->value(l)==l_True){
					//only pop a decision that was actually made
					to_decide.pop();
				}else if(outer->value(l)==l_False){
					//is this even a reachable state? it probably shouldn't be.
					to_decide.clear();
					path_edges.clear();
					//needs recompute!
					return decide(decision_reason);
				}
            }
        }

        return lit_Undef;
    }
};


template<typename Weight>
void ReachDetector<Weight>::addLit(int from, int to, Var outer_reach_var) {
	while (reach_lits.size() < g_under.nodes())
		reach_lits.push(lit_Undef);
	while (original_reach_lits.size() < g_under.nodes())
		original_reach_lits.push(false);

	original_reach_lits[to] = true;
	if (reach_lits[to] != lit_Undef) {
		Lit r = reach_lits[to];
		//force equality between the new lit and the old reach lit, in the SAT solver
		outer->makeEqualInSolver(outer->toSolver(r), mkLit(outer_reach_var));
		return;
	}

	g_under.invalidate();
	g_over.invalidate();

	Var reach_var = outer->newVar(outer_reach_var, getID());

	if (first_reach_var == var_Undef) {
		first_reach_var = reach_var;
	} else {
		assert(reach_var >= first_reach_var);
	}
	Lit reachLit = mkLit(reach_var, false);
	assert(reach_lits[to] == lit_Undef);
	//if(reach_lits[to]==lit_Undef){
	reach_lits[to] = reachLit;
	while (reach_lit_map.size() <= reach_var - first_reach_var) {
		reach_lit_map.push(-1);
	}
	reach_lit_map[reach_var - first_reach_var] = to;
	if(underapprox_detector){
		underapprox_detector->addDestination(to);
	}
	if(overapprox_reach_detector){
		overapprox_reach_detector->addDestination(to);
	}
	if(overapprox_path_detector!=overapprox_reach_detector){
		overapprox_path_detector->addDestination(to);
	}
	assert(from == source);
	if (opt_encode_reach_underapprox_as_sat || !underapprox_detector) {
		buildSATConstraints(true);
		if(cnf_reach_lits[to]!=lit_Undef && cnf_reach_lits[to]!=reach_lits[to]){
			Lit r = cnf_reach_lits[to];
			//force equality between the new lit and the old reach lit, in the SAT solver
			outer->makeEqualInSolver(outer->toSolver(r), mkLit(outer_reach_var));
			return;
		}

	}
	if (!overapprox_reach_detector) {
		buildSATConstraints(false);
	}
    if(opt_decide_theories && opt_allow_reach_decisions && overapprox_reach_detector && !is_edge_set_detector){
        Heuristic * h = new ReachHeuristic<Weight>(outer,this,reachLit,to);
        reach_heuristics.growTo(to+1,nullptr);
        reach_heuristics[to]=h;
        all_reach_heuristics.push(h);
    }


	if (opt_conflict_min_cut || opt_adaptive_conflict_mincut) {
		if (!opt_reach_detector_combined_maxflow) {
			conflict_flows.resize(g_under.nodes(), nullptr);
			for (int i = 0; i < g_under.nodes(); i++) {
				if (reach_lits[i] != lit_Undef && !conflict_flows[i]) {
					MaxFlow<int64_t> * conflict_flow_t = nullptr;
					if (mincutalg == MinCutAlg::ALG_EDKARP_DYN) {
						conflict_flow_t = new EdmondsKarpDynamic< int64_t>(outer->cutGraph,  source,
								i);
					} else if (mincutalg == MinCutAlg::ALG_EDKARP_ADJ) {

						conflict_flow_t = new EdmondsKarpAdj< int64_t>(outer->cutGraph,  source, i);

					} else if (mincutalg == MinCutAlg::ALG_DINITZ) {

						conflict_flow_t = new Dinitz< int64_t>(outer->cutGraph,  source, i);

					} else if (mincutalg == MinCutAlg::ALG_DINITZ_LINKCUT) {
						//link-cut tree currently only supports ints (enforcing this using tempalte specialization...).

						conflict_flow_t = new Dinitz< int64_t>(outer->cutGraph,  source, i);

					} else if (mincutalg == MinCutAlg::ALG_KOHLI_TORR) {
						if (opt_use_kt_for_conflicts) {
							conflict_flow_t = new KohliTorr< int64_t>(outer->cutGraph,  source, i,
									opt_kt_preserve_order);
						} else
							conflict_flow_t = new EdmondsKarpDynamic< int64_t>(outer->cutGraph,
									source, i);
					} else {

						conflict_flow_t = new EdmondsKarpAdj< int64_t>(outer->cutGraph,  source, i);

					}
					conflict_flows[i] = conflict_flow_t;
				}
			}
		}
	}
}
template<typename Weight>
void ReachDetector<Weight>::ReachStatus::setReachable(int u, bool reachable) {

	if (polarity == reachable && u < detector.reach_lits.size()) {
		Lit l = detector.reach_lits[u];
		if (l != lit_Undef && polarity && !detector.is_changed_under[u]) {
			lbool assign = detector.outer->value(l);
			if (assign != (reachable ? l_True : l_False)) {
				detector.is_changed_under[u] = true;
				detector.changed.push( { var(l), u,polarity });
			}
		}else if (l != lit_Undef && !polarity && !detector.is_changed_over[u]) {
			lbool assign = detector.outer->value(l);
			if (assign != (reachable ? l_True : l_False)) {
				detector.is_changed_over[u] = true;
				detector.changed.push( { var(l), u,polarity });
			}
		}
	}
}
template<typename Weight>
void ReachDetector<Weight>::ReachStatus::setMininumDistance(int u, bool reachable, Weight distance) {
	/*assert(reachable ==(distance<detector.outer->g.nodes()));
	 setReachable(u,reachable);

	 if(u<detector.dist_lits.size()){
	 assert(distance>=0);
	 assert(distance<detector.outer->g.nodes());
	 for(int i = 0;i<detector.dist_lits[u].size();i++){
	 int d =  detector.dist_lits[u][i].min_distance;
	 Lit l = detector.dist_lits[u][i].l;
	 assert(l!=lit_Undef);
	 if(d<distance && !polarity){
	 lbool assign = detector.outer->value(l);
	 if( assign!= l_False ){
	 detector.changed.push({~l,u});
	 }
	 }else if(d>=distance && polarity){
	 lbool assign = detector.outer->value(l);
	 if( assign!= l_True ){
	 detector.changed.push({l,u});
	 }
	 }
	 }
	 }*/
}
/*template<typename Weight>
 bool ReachDetector<Weight>::ChokepointStatus::mustReach(int node){
 Lit l =  detector.reach_lits[node];
 if(l!=lit_Undef){
 return detector.outer->value(l)==l_True;
 }
 return false;
 }
 template<typename Weight>
 bool ReachDetector<Weight>::ChokepointStatus::operator() (int edge_id){
 return detector.outer->value(detector.outer->edge_list[ edge_id].v)==l_Undef;
 }*/
template<typename Weight>
void ReachDetector<Weight>::preprocess() {
	is_changed_under.growTo(g_under.nodes());
	is_changed_over.growTo(g_over.nodes());
	//vec<bool> pure;
	//pure.growTo(reach_lits.size());
	//can check if all reach lits appear in only one polarity in the solver constraints; if so, then we can disable either check_positive or check_negative
}
template<typename Weight>
void ReachDetector<Weight>::buildReachReason(int node, vec<Lit> & conflict) {
	//drawFull();
	Reach & d = *underapprox_path_detector;
	double starttime = rtime(2);
	d.update();

	assert(outer->dbg_reachable(d.getSource(), node));

	assert(d.connected_unchecked(node));
	if (opt_learn_reaches == 0 || opt_learn_reaches == 2) {
		int u = node;
		int p;
		while ((p = d.previous(u)) != -1) {
			Edge & edg = outer->edge_list[d.incomingEdge(u)]; //outer->edges[p][u];
			Var e = edg.v;
			lbool val = outer->value(e);
			assert(outer->value(e)==l_True);
			conflict.push(mkLit(e, true));
			u = p;

		}
	} else {
		//Instead of a complete path, we can learn reach variables, if they exist
		int u = node;
		int p;
		while ((p = d.previous(u)) != -1) {
			Edge & edg = outer->edge_list[d.incomingEdge(u)]; //outer->edges[p][u];
			Var e = edg.v;
			lbool val = outer->value(e);
			assert(outer->value(e)==l_True);
			conflict.push(mkLit(e, true));
			u = p;
			if (u < reach_lits.size() && reach_lits[u] != lit_Undef && outer->value(reach_lits[u]) == l_True
					&& outer->level(var(reach_lits[u])) < outer->decisionLevel()) {
				//A potential (fixed) problem with the above: reach lit can be false, but have been assigned after r in the trail, messing up clause learning if this is a reason clause...
				//This is avoided by ensuring that L is lower level than the conflict.
				Lit l = reach_lits[u];
				assert(outer->value(l)==l_True);
				conflict.push(~l);
				break;
			}
		}

	}
	stats_under_conflicts++;
	outer->num_learnt_paths++;
	outer->learnt_path_clause_length += (conflict.size() - 1);

	stats_under_clause_length += (conflict.size() - 1);
	double elapsed = rtime(2) - starttime;
	outer->pathtime += elapsed;
	stats_under_conflict_time += elapsed;
}
template<typename Weight>
void ReachDetector<Weight>::buildNonReachReason(int node, vec<Lit> & conflict, bool force_maxflow) {
	static int it = 0;
	++it;
	if (it == 4) {
		int a = 1;
	}
	int u = node;
	stats_over_conflicts++;
	//drawFull( non_reach_detectors[detector]->getSource(),u);
	assert(outer->dbg_notreachable(source, u));
	//assert(!negative_reach_detector->connected_unchecked(node));
	double starttime = rtime(2);
	/*if(opt_verb>1){
		printf("Reach conflict %d, graph %d\n", it, outer->getTheoryIndex());
	}*/
	if ((force_maxflow || opt_conflict_min_cut) && (conflict_flow || conflict_flows[node])) {

		//g_over.drawFull();
		cut.clear();
		int64_t f;
		if (!conflict_flow) {
			assert(conflict_flows[node]->getSink() == node);
			assert(conflict_flows[node]->getSource() == source);

			f = conflict_flows[node]->minCut(cut);
		} else {
			assert(conflict_flow->getSource() == source);
			conflict_flow->setSink(node);
			f = conflict_flow->minCut(cut);
		}
		assert(f == cut.size());						//because edges are only ever infinity or 1

		for (int i = 0; i < cut.size(); i++) {
			MaxFlowEdge e = cut[i];
			int cut_id = e.id;
			assert(cut_id % 2 == 0);
			Lit l = mkLit(outer->getEdgeVar(cut_id / 2), false);
			assert(outer->value(l)==l_False);
			conflict.push(l);
		}
		/*				{
		 EdmondsKarpAdj<CutStatus,int64_t> ek(outer->cutGraph, cutStatus,source,node);
		 std::vector<MaxFlowEdge> tmpcut;
		 int64_t tf = ek.minCut(tmpcut);
		 printf("cut size:%d, %d, expected: %d, %d \n",cut.size(),f, tmpcut.size(), tf);
		 if(f != tf || cut.size()!= tmpcut.size()){
		 exit(3);
		 }
		 }*/
	} else {
		//We could learn an arbitrary (non-infinite) cut here, or just the whole set of false edges
		//or perhaps we can learn something equivalent to the 1-uip cut?

		vec<int>& to_visit = outer->to_visit;
		vec<char>& seen = outer->seen;

		to_visit.clear();
		to_visit.push(node);
		seen.clear();
		seen.growTo(outer->nNodes());
		seen[node] = true;

		do {

			assert(to_visit.size());
			int u = to_visit.last();
			assert(u != source);
			to_visit.pop();
			assert(seen[u]);
			assert(outer->dbg_notreachable(source, u));
			//assert(!negative_reach_detector->connected_unsafe(u));
			//Ok, then add all its incoming disabled edges to the cut, and visit any unseen, non-disabled incoming.edges()
			for (int i = 0; i < outer->inv_adj[u].size(); i++) {
				int v = outer->inv_adj[u][i].v;
				int from = outer->inv_adj[u][i].from;
				int edge_num = outer->getEdgeID(v);				    	// v-outer->min_edge_var;
				if (from == u) {
					assert(outer->edge_list[edge_num].to == u);
					assert(outer->edge_list[edge_num].from == u);
					continue;				  //Self loops are allowed, but just make sure nothing got flipped around...
				}
				assert(from != u);
				assert(outer->inv_adj[u][i].to == u);
				//Note: the variable has to not only be assigned false, but assigned false earlier in the trail than the reach variable...

				if (outer->value(v) == l_False) {
					//note: we know we haven't seen this edge variable before, because we know we haven't visited this node before
					//if we are already planning on visiting the from node, then we don't need to include it in the conflict (is this correct?)
					//if(!seen[from])
					conflict.push(mkLit(v, false));
				} else {
					assert(from != source);
					//even if it is undef? probably...
					if (!seen[from]) {
						seen[from] = true;
						if ((opt_learn_reaches == 2 || opt_learn_reaches == 3) && from < reach_lits.size()
								&& reach_lits[from] != lit_Undef && outer->value(reach_lits[from]) == l_False
								&& outer->level(var(reach_lits[from])) < outer->decisionLevel()) {
							//The problem with the above: reach lit can be false, but have been assigned after r in the trail, messing up clause learning if this is a reason clause...
							Lit r = reach_lits[from];
							assert(var(r) < outer->nVars());
							assert(outer->value(r)==l_False);
							conflict.push(r);
						} else
							to_visit.push(from);
					}
				}
			}
		} while (to_visit.size());

	}

	if (opt_shrink_theory_conflicts) {
		//visit each edge lit in this initial conflict, and see if unreachability is preserved if we add the edge back in (temporarily)
		/*	int i,j=0;
		 outer->cutGraph.clearHistory();
		 outer->cutGraph.invalidate();
		 while(cutgraph.nodes()<g.nodes()){
		 cutgraph.addNode();
		 }
		 while(cutgraph.nEdgeIDs()<g.nEdgeIDs()){
		 //if an edge hasn't been disabled at level 0, add it here.
		 int edgeID = cutgraph.nEdgeIDs();
		 Var v = outer->getEdgeVar(edgeID);
		 Edge & e = outer->edge_list[edgeID];
		 cutgraph.addEdge(e.from,e.to,edgeID);
		 if(outer->value(v)==l_False && outer->level(v)==0){
		 //permanently disabled edge
		 cutgraph.disableEdge(edgeID);
		 }
		 }
		 //cutgraph.drawFull();
		 #ifdef DEBUG_GRAPH
		 for(int i = 0;i<g.nEdgeIDs();i++){
		 Var v = outer->getEdgeVar(i);
		 if(outer->value(v)==l_False && outer->level(v)==0){

		 }else
		 assert(cutgraph.edgeEnabled(i));
		 }
		 #endif
		 removed_edges.clear();
		 for(i = 0;i<conflict.size();i++){
		 Lit l = conflict[i];
		 if(!sign(l) && outer->isEdgeVar(var(l))){
		 int edgeID = outer->getEdgeID(var(l));
		 cutgraph.disableEdge(edgeID);
		 removed_edges.push(edgeID);
		 }
		 }
		 //cutgraph.drawFull();
		 for(i = 0;i<conflict.size();i++){
		 Lit l = conflict[i];
		 if(!sign(l) && outer->isEdgeVar(var(l))){
		 //check if the target is still unreachable if we add this lit back in
		 int edgeID = outer->getEdgeID(var(l));
		 assert(!antig.edgeEnabled(edgeID));
		 assert(!cutgraph.edgeEnabled(edgeID));
		 assert(!cutgraph_reach_detector->connected(node));
		 cutgraph.enableEdge(edgeID);
		 if(cutgraph_reach_detector->connected(node)){
		 cutgraph.disableEdge(edgeID);
		 conflict[j++]=l;
		 }else{
		 //we can drop this edge from the conflict.
		 stats_shrink_removed++;
		 }
		 }else{
		 conflict[j++]=l;
		 }

		 }
		 conflict.shrink(i-j);
		 //restore the state of the graph
		 for(int edgeID: removed_edges){
		 cutgraph.enableEdge(edgeID);
		 }

		 */

	}

	if (!force_maxflow && opt_adaptive_conflict_mincut > 0 && (conflict.size() - 1 > opt_adaptive_conflict_mincut)
			&& (conflict_flow || conflict_flows[node])) { //-1 to ignore the predicate's literal stored at position 0
		conflict.shrink(conflict.size() - 1);
		assert(conflict.size() == 1);
		buildNonReachReason(node, conflict, true);
		return;
	}

	if (opt_learn_unreachable_component && conflict.size() > 1) {
		//Taking a page from clasp, instead of just learning that node u is unreachable unless one of these edges is flipped,
		//we are going to learn that the whole strongly connected component attached to u is unreachable (if that component has more than one node, that is)
		assert(conflict[0] == ~reach_lits[node]);
		std::vector<int> component;
		vec<Lit> reach_component;
		/*DFSReachability<> d(u,g);

		 */
		//antig.drawFull();
		TarjansSCC<Weight>::getSCC(node, g_over, component);
		assert(std::count(component.begin(), component.end(), node));
		for (int n : component) {
			if (reach_lits[n] != lit_Undef) {
				reach_component.push(reach_lits[n]);
			}
		}
		assert(reach_component.size());
		if (reach_component.size() > 1) {
			//create a new literal in the solver

			//component must be reachable
			bool must_be_reached = false;
			for (Lit l : reach_component) {
				if (outer->value(l) == l_True && outer->level(var(l)) == 0) {
					must_be_reached = true;
				}
			}
			extra_conflict.clear();

			if (must_be_reached) {

				for (int i = 1; i < conflict.size(); i++)
					extra_conflict.push(conflict[i]);
				outer->addClauseSafely(extra_conflict);
				stats_learnt_components++;
				stats_learnt_components_sz += reach_component.size();
			} else {

				stats_learnt_components++;
				stats_learnt_components_sz += reach_component.size();
				Lit component_reachable = mkLit(outer->newVar());
				conflict.copyTo(extra_conflict);
				extra_conflict[0] = ~component_reachable;

				outer->addClauseSafely(extra_conflict);

				for (Lit l : reach_component) {
					if (outer->value(l) == l_True && outer->level(var(l)) == 0) {
						continue;
					}
					extra_conflict.clear();
					extra_conflict.push(component_reachable);
					extra_conflict.push(~l);
					outer->addClauseSafely(extra_conflict);
				}
			}
		} else {
			//just learn the normal conflict clause
		}
	}

	stats_over_clause_length += (conflict.size() - 1);
	outer->learnt_cut_clause_length += (conflict.size() - 1);

	double elapsed = rtime(2) - starttime;
	outer->mctime += elapsed;
	stats_over_conflict_time += elapsed;

}

/**
 * Explain why an edge was forced (to true).
 * The reason is that _IF_ that edge is false, THEN there is a cut of disabled edges between source and target
 * So, create the graph that has that edge (temporarily) assigned false, and find a min-cut in it...
 */
template<typename Weight>
void ReachDetector<Weight>::buildForcedEdgeReason(int reach_node, int forced_edge_id, vec<Lit> & conflict) {
	static int it = 0;
	++it;

	assert(outer->value(outer->edge_list[forced_edge_id].v)==l_True);
	Lit edgeLit = mkLit(outer->edge_list[forced_edge_id].v, false);

	conflict.push(edgeLit);

	int forced_edge_from = outer->edge_list[forced_edge_id].from;
	int forced_edge_to = outer->edge_list[forced_edge_id].to;

	int u = reach_node;
	//drawFull( non_reach_detectors[detector]->getSource(),u);
	assert(outer->dbg_notreachable(source, u));
	double starttime = rtime(2);
	outer->cutGraph.clearHistory();
	outer->stats_mc_calls++;

	/*if(opt_conflict_min_cut){
	 if(mincutalg!= MinCutAlg::ALG_EDKARP_ADJ){
	 //ok, set the weights for each edge in the cut graph.
	 //Set edges to infinite weight if they are undef or true, and weight 1 otherwise.
	 for(int u = 0;u<outer->cutGraph.nodes();u++){
	 for(int j = 0;j<outer->cutGraph.nIncident(u);j++){
	 int v = outer->cutGraph.incident(u,j).node;
	 int edgeid =  outer->cutGraph.incident(u,j).id;
	 Var var = outer->getEdgeVar(edgeid);
	 //Var var = outer->edges[u][v].v;
	 if(S->value(var)==l_False){
	 mc.setCapacity(u,v,1);
	 }else{
	 outer->mc->setCapacity(u,v,0xF0F0F0);
	 //}
	 }
	 }

	 //find any edges assigned to false, and set their capacity to 1
	 for(int i =0;i<outer->trail.size();i++){
	 if(outer->trail[i].isEdge && !outer->trail[i].assign){
	 outer->mc->setCapacity(outer->trail[i].from, outer->trail[i].to,1);
	 }
	 }

	 outer->mc->setCapacity(forced_edge_from, forced_edge_to,1);
	 }
	 outer->cut.clear();

	 int f =outer->mc->minCut(source,reach_node,outer->cut);
	 assert(f<0xF0F0F0); assert(f==outer->cut.size());//because edges are only ever infinity or 1
	 for(int i = 0;i<outer->cut.size();i++){
	 MaxFlowEdge e = outer->cut[i];

	 Lit l = mkLit(outer->getEdgeVar(e.id),false);
	 assert(outer->value(l)==l_False);
	 conflict.push(l);
	 }
	 }else*/{
		//We could learn an arbitrary (non-infinite) cut here, or just the whole set of false edges
		//or perhaps we can learn the actual 1-uip cut?

		vec<int>& to_visit = outer->to_visit;
		vec<char>& seen = outer->seen;

		to_visit.clear();
		to_visit.push(reach_node);
		seen.clear();
		seen.growTo(outer->nNodes());
		seen[reach_node] = true;

		do {

			assert(to_visit.size());
			int u = to_visit.last();
			assert(u != source);
			to_visit.pop();
			assert(seen[u]);
			assert(!overapprox_reach_detector->connected_unsafe(u));
			//Ok, then add all its incoming disabled edges to the cut, and visit any unseen, non-disabled incoming.edges()
			for (int i = 0; i < outer->inv_adj[u].size(); i++) {
				int v = outer->inv_adj[u][i].v;
				int from = outer->inv_adj[u][i].from;
				assert(from != u);
				assert(outer->inv_adj[u][i].to == u);
				//Note: the variable has to not only be assigned false, but assigned false earlier in the trail than the reach variable...
				int edge_num = outer->getEdgeID(v);						// v-outer->min_edge_var;

				if (edge_num == forced_edge_id || outer->value(v) == l_False) {
					//note: we know we haven't seen this edge variable before, because we know we haven't visited this node before
					//if we are already planning on visiting the from node, then we don't need to include it in the conflict (is this correct?)
					//if(!seen[from])
					conflict.push(mkLit(v, false));
				} else {
					assert(from != source);
					//even if it is undef? probably...
					if (!seen[from]) {
						seen[from] = true;
						if ((opt_learn_reaches == 2 || opt_learn_reaches == 3) && from < reach_lits.size()
								&& reach_lits[from] != lit_Undef && outer->value(reach_lits[from]) == l_False
								&& outer->level(var(reach_lits[from])) < outer->decisionLevel()) {
							//The problem with the above: reach lit can be false, but have been assigned after r in the trail, messing up clause learning if this is a reason clause...
							Lit r = reach_lits[from];
							assert(var(r) < outer->nVars());
							assert(outer->value(r)==l_False);
							conflict.push(r);
						} else
							to_visit.push(from);
					}
				}
			}
		} while (to_visit.size());

	}

	outer->num_learnt_cuts++;
	outer->learnt_cut_clause_length += (conflict.size() - 1);

	double elapsed = rtime(2) - starttime;
	outer->mctime += elapsed;

}
template<typename Weight>
void ReachDetector<Weight>::buildReason(Lit p, vec<Lit> & reason, CRef marker) {
	if (marker == underprop_marker) {
		reason.push(p);
		//	double startpathtime = rtime(2);

		/*Dijkstra & detector = *reach_detectors[d]->positive_dist_detector;
		 //the reason is a path from s to p(provided by d)
		 //p is the var for a reachability detector in dijkstra, and corresponds to a node
		 detector.update();
		 Var v = var(p);
		 int u =reach_detectors[d]->getNode(v); //reach_detectors[d]->reach_lit_map[v];
		 assert(detector.connected(u));
		 int w;
		 while(( w= detector.previous(u)) > -1){
		 Lit l = mkLit( edges[w][u].v,true );
		 assert(S->value(l)==l_False);
		 reason.push(l);
		 u=w;
		 }*/
		Var v = var(p);
		int u = getNode(v);
		buildReachReason(u, reason);

		//double elapsed = rtime(2)-startpathtime;
		//	pathtime+=elapsed;
	} else if (marker == overprop_marker) {
		reason.push(p);

		//the reason is a cut separating p from s;
		//We want to find a min-cut in the full graph separating, where activated edges (ie, those still in antig) are weighted infinity, and all others are weighted 1.

		//This is a cut that describes a minimal set of edges which are disabled in the current graph, at least one of which would need to be activated in order for s to reach p
		//assign the mincut edge weights if they aren't already assigned.

		Var v = var(p);
		int t = getNode(v); // v- var(reach_lits[d][0]);
		buildNonReachReason(t, reason);

	} else if (marker == forced_edge_marker) {
		Var v = var(p);
		//The forced variable is an EDGE that was forced.
		int forced_edge_id = outer->getEdgeID(v); //v- outer->min_edge_var;
		//The corresponding node that is the reason it was forced
		int reach_node = force_reason[forced_edge_id];
		buildForcedEdgeReason(reach_node, forced_edge_id, reason);
	} else {
		assert(false);
	}
	outer->toSolver(reason);
}
template<typename Weight>
bool ReachDetector<Weight>::propagate(vec<Lit> & conflict) {
	static int iter = 0;
	if (++iter == 2374) {
		int a = 1;
	}
    conflictingHeuristic=nullptr;
	bool skipped_positive = false;
	if (underapprox_detector && (!opt_detect_pure_theory_lits || unassigned_positives > 0)) {
		double startdreachtime = rtime(2);
		stats_under_updates++;
		underapprox_detector->update();
		double reachUpdateElapsed = rtime(2) - startdreachtime;
		//outer->reachupdatetime+=reachUpdateElapsed;
		stats_under_update_time += rtime(2) - startdreachtime;
	} else {
		skipped_positive = true;
		//outer->stats_pure_skipped++;
		stats_skipped_under_updates++;
	}
	bool skipped_negative = false;
	if (overapprox_reach_detector && (!opt_detect_pure_theory_lits || unassigned_negatives > 0)) {
		double startunreachtime = rtime(2);
		stats_over_updates++;
		overapprox_reach_detector->update();
		double unreachUpdateElapsed = rtime(2) - startunreachtime;
		//outer->unreachupdatetime+=unreachUpdateElapsed;
		stats_over_update_time += rtime(2) - startunreachtime;
	} else {
		skipped_negative = true;
		stats_skipped_over_updates++;
	}

	if (opt_rnd_shuffle) {
		randomShuffle(rnd_seed, changed);
	}

	while (changed.size()) {
		int sz = changed.size();
		Change & ch = changed.last();
		bool polarity = ch.polarity;
		Var v =ch.v;
		int u = ch.u;
		assert(polarity ? is_changed_under[u]:is_changed_over[u]);
		Lit l;

		if (underapprox_detector && polarity && underapprox_detector->connected(u)) {
			l = mkLit(v, false);
		} else if (overapprox_reach_detector && !polarity && !overapprox_reach_detector->connected(u)) {
			l = mkLit(v, true);
		} else {

			if(sz == changed.size()){
				//it is possible, in rare cases, for literals to have been added to the chagned list while processing the changed list.
				//in that case, don't pop anything off the changed list, and instead accept that this literal may be re-processed
				//(should only be possible to have this happen at most twice per propagation call, so shouldn't matter too much)
				assert(sz == changed.size());
				assert(ch.u == u);
				if (polarity) {
					is_changed_under[u] = false;
				} else {
					is_changed_over[u] = false;
				}
				changed.pop();
			}
			//this can happen if the changed node's reachability status was reported before a backtrack in the solver.
			continue;
		}

		bool reach = !sign(l);
		if (outer->value(l) == l_True) {
			//do nothing
		} else if (outer->value(l) == l_Undef) {

			//trail.push(Assignment(false,reach,detectorID,0,var(l)));
			if (reach)
				outer->enqueue(l, underprop_marker);
			else
				outer->enqueue(l, overprop_marker);
		} else if (outer->value(l) == l_False) {
			conflict.push(l);
            conflictingHeuristic= u < reach_heuristics.size() ?  reach_heuristics[u]:nullptr;
     /*       if(opt_verb>=4){
                drawGrid(g_over, u);
                drawGrid(g_under, u);
            }*/
			if (reach) {

				//conflict
				//The reason is a path in g from to s in d
				buildReachReason(u, conflict);
				//add it to s
				//return it as a conflict

			} else {
				//The reason is a cut separating s from t
				buildNonReachReason(u, conflict);

			}

			outer->toSolver(conflict);
			return false;
		}

		/*if(opt_reach_prop){
		 forced_edges.clear();
		 chokepoint.collectForcedEdges(forced_edges);
		 for(int i = 0;i<forced_edges.size();i++){
		 int edge_id = forced_edges[i].edge_id;
		 int node = forced_edges[i].node;
		 Lit l = mkLit( outer->edge_list[edge_id].v,false);
		 if(outer->value(l)==l_Undef){
		 force_reason.growTo(edge_id+1);
		 force_reason[edge_id]=node;
		 outer->enqueue(l,forced_reach_marker);
		 }else if(outer->value(l)==l_True){
		 //do nothing

		 }else{
		 //conflict.
		 //this actually shouldn't be possible (at this point in the code)
		 buildForcedEdgeReason(node,edge_id,conflict);
		 return false;
		 }
		 }

		 }*/
		if(sz==changed.size()) {
			//it is possible, in rare cases, for literals to have been added to the chagned list while processing the changed list.
			//in that case, don't pop anything off the changed list, and instead accept that this literal may be re-processed
			//(should only be possible to have this happen at most twice per propagation call, so shouldn't matter too much)
			assert(sz ==
				   changed.size());//This can be really tricky - if you are not careful, an a reach detector's update phase was skipped at the beginning of propagate, then if the reach detector is called during propagate it can push a change onto the list, which can cause the wrong item to be removed here.
			assert(ch.u == u);
			if (polarity) {
				is_changed_under[u] = false;
			} else {
				is_changed_over[u] = false;
			}
			changed.pop();
		}
	}

#ifdef DEBUG_GRAPH
	for(int i = 0;i<reach_lits.size();i++) {
		Lit l = reach_lits[i];
		if(l!=lit_Undef) {
			lbool val = outer->value(l);
			int u = getNode(var(l));
			if((underapprox_detector && (!opt_detect_pure_theory_lits || unassigned_positives>0) && underapprox_detector->connected_unsafe(u))) {
				assert(outer->value(l)==l_True);
				assert(outer->dbg_value(l)==l_True);
			} else if (overapprox_reach_detector && ((!opt_detect_pure_theory_lits || unassigned_negatives>0) && !overapprox_reach_detector->connected_unsafe(u))) {
				assert(outer->value(l)==l_False);
				assert(outer->dbg_value(l)==l_False);
			}
		}

	}
#endif
	return true;
}

template<typename Weight>
void ReachDetector<Weight>::printSolution(std::ostream & write_to) {

	vec<bool> to_show;
	to_show.growTo(g_under.nodes());

	for (int i = 0; i < reach_lits.size(); i++) {
		if (!original_reach_lits[i])//only print paths to nodes that the user asked for (this matters if we are using a CNF reachability encoding, which may have invented extra reach lits)
			continue;
		Lit l = reach_lits[i];
		if (l != lit_Undef) {
			int to = reach_lit_map[var(l) - first_reach_var];
			to_show[to] = true;
		}
	}
	vec<int> path;
	for (int to = 0; to < g_under.nodes(); to++) {
		if (!to_show[to])
			continue;

		Reach & d = *underapprox_path_detector;
		d.update();
		if (d.connected(to)) {
			write_to << "Path from " << source << "->" << to << " is : ";
			path.clear();
			int u = to;
			path.push(u);
			int p;
			while ((p = d.previous(u)) != -1) {
				Edge & edg = outer->edge_list[d.incomingEdge(u)]; //outer->edges[p][u];
				path.push(p);
				u = p;
			}

			for (int i = path.size() - 1; i >= 0; i--) {
				write_to << path[i] << ",";
			}
			write_to << '\n';
		} else {
			write_to << "No path from" << source << "->" << to << "\n";
		}
	}

}
template<typename Weight>
bool ReachDetector<Weight>::checkSatisfied() {

	UnweightedDijkstra<Weight> under(source, g_under);
	UnweightedDijkstra<Weight> over(source, g_over);
	under.update();
	over.update();

	for (int j = 0; j < reach_lits.size(); j++) {
		Lit l = reach_lits[j];
		if (l != lit_Undef) {
			int node = j;
			if (outer->value(l) == l_True) {
				if (!under.connected(node)) {
					return false;
				}
			} else if (outer->value(l) == l_False) {
				if (over.connected(node)) {
					return false;
				}
			} else {
				if (over.connected(node)) {
					return false;
				}
				if (!under.connected(node)) {
					return false;
				}
			}
		}
	}

	return true;
}
template<typename Weight>
void ReachDetector<Weight>::dbg_sync_reachability() {
#ifdef DEBUG_GRAPH
	if (!underapprox_detector)
		return;
	for (int j = 0; j < reach_lits.size(); j++) {
		Lit l = reach_lits[j];
		if (l != lit_Undef) {
			int node = getNode(var(l));

			if (underapprox_detector->connected(node)) {
				assert(outer->value(l)==l_True);
			} else if (!overapprox_reach_detector->connected(node)) {
				assert(outer->value(l)==l_False);
			}
		}

	}
#endif
}

/*
 int ReachDetector<Weight>::OptimalWeightEdgeStatus::operator [] (int edge) const {
 Var v = detector.outer->edge_list[edge].v;
 lbool val = detector.outer->value(v);
 if(val==l_False){
 assert(false);
 return detector.outer->edge_list.size()*2;
 }else if (val==l_True){
 return 0;
 }else{
 return 1;
 }
 }
 int ReachDetector<Weight>::OptimalWeightEdgeStatus::size()const{
 return detector.outer->edge_list.size();
 }*/

template<typename Weight>
Lit ReachDetector<Weight>::decide(CRef &decision_reason) {
	return lit_Undef;

}
;




template<typename Weight>
class FlowPathHeuristic : public Heuristic{

    ReachDetector<Weight> * r;
    MaxflowDetector<Weight> * mf;
    vec<Lit> to_decide;

    GraphTheorySolver<Weight> *outer;
    Lit reach_lit = lit_Undef;
    int last_over_modification=-1;
    int last_over_addition=-1;
    int last_over_deletion=-1;
    int over_history_qhead=0;
    int last_over_history_clear=0;

    int last_under_history_clear=0;
    int under_history_qhead=0;
    int last_under_modification=-1;
    int last_under_addition=-1;
    int last_under_deletion=-1;

    DynamicGraph<Weight> & g_over;
    DynamicGraph<Weight> & g_under;
    DynamicGraph<Weight> & f_over;
    DynamicGraph<Weight> & f_under;
    IntSet<int> path_edges;
    bool path_is_cut=false;
    int dest=-1;
    int  source=-1;
    vec<int> q;
    vec<char> seen;
    vec<int> prev;
    DynamicGraph<Weight> * flow_graph;
    UnweightedRamalReps<Weight> * flow_reach;

public:
    FlowPathHeuristic(GraphTheorySolver<Weight> * outer, ReachDetector<Weight> * r, MaxflowDetector<Weight> * mf, Lit reach_lit, Lit maxflow_lit,int dest_node): r(r),mf(mf),outer(outer),reach_lit(reach_lit),g_over(r->g_over),g_under(r->g_under),f_over(mf->g_over),f_under(mf->g_under),dest(dest_node) {
        this->setPriority(outer->getSolver()->getDecisionPriority(var(outer->toSolver(reach_lit))));
        source = r->source;
        assert(f_under.nodes()>source);
        assert(f_under.nodes()>dest);

        MaxFlow<Weight> *  overapprox = mf->overapprox_conflict_detector;
        flow_graph = overapprox->getFlowGraph();
        flow_reach = new UnweightedRamalReps<Weight>(source,*flow_graph);

    }


    void computePath(){
        path_is_cut=false;

        //plan:
        //1) assume that the maxflow graph has the same (or a superset) of the edges/nodes as the reach graph, even if they are different graph theory instances
        //2) IF the maxflow overapprox flow happens to have a path from s to t, then we're going to attempt to treat that as the reach decision path
        MaxFlow<Weight> *  overapprox = mf->overapprox_conflict_detector;

        to_decide.clear();
        path_edges.clear();


        {
            Lit l = reach_lit;
            assert (l != lit_Undef);

            int j = r->getNode(var(l));
            if (outer->value(l) == l_True) {
                overapprox->update();
                flow_reach->update();


                    //then lets try to connect this

                    to_decide.clear();


                    if(flow_reach->connected(j)) {
                        int p = j;
                        int last_edge = -1;
                        int last = j;

                        {
                            //ok, read back the path from the over to find a candidate edge we can decide
                            //find the earliest unconnected node on this path


                            p = j;
                            last = j;
                            //while (!under_reach->connected(p)) {
                            while (p != source) {
                                last = p;
                                assert(p != source);
                                last_edge = flow_reach->incomingEdge(p);
                                assert(last_edge>=0);
                                int expected_from = f_over.getEdge(last_edge).from;
                                int expected_to = f_over.getEdge(last_edge).to;

                                if (!g_over.hasEdge(last_edge) || expected_from!= g_over.getEdge(last_edge).from || expected_to!= g_over.getEdge(last_edge).to){
                                    throw std::runtime_error("Missing edge for flow heuristic");
                                }

                                path_edges.insert(last_edge);
                                Var edge_var = outer->getEdgeVar(last_edge);
                                if (outer->value(edge_var) == l_Undef) {

                                    to_decide.push(mkLit(edge_var, false));
                                }
                                int prev = flow_reach->previous(p);
                                p = prev;

                            }
                        }


                    }

            }

        }

    }

    bool needsRecompute(){
        if (outer->value(reach_lit) != l_False && path_is_cut) {
            return true;
        }else if (outer->value(reach_lit) != l_True && !path_is_cut) {
            return true;
        }


        //check if any edges on the path have been removed since the last update
        if (last_over_modification > 0 && g_over.modifications == last_over_modification && last_under_modification > 0 && g_under.modifications == last_under_modification){
            return false;
        }
        if (last_over_modification <= 0 || g_over.changed() || last_under_modification <= 0 || g_under.changed()) {//Note for the future: there is probably room to improve this further.
            return true;
        }

        if (last_over_history_clear != g_over.historyclears || last_under_history_clear != g_under.historyclears) {
            over_history_qhead = g_over.historySize();
            last_over_history_clear = g_over.historyclears;
            under_history_qhead = g_under.historySize();
            last_under_history_clear = g_under.historyclears;
            to_decide.clear();
            for(int edgeID:path_edges){
                if(!path_is_cut) {
                    if (!g_over.edgeEnabled(edgeID))
                        return true;
                    Var edge_var = outer->getEdgeVar(edgeID);
                    if (outer->value(edge_var) == l_Undef) {
                        to_decide.push(mkLit(edge_var, false));
                    }
                }else{
                    if (g_under.edgeEnabled(edgeID))
                        return true;
                    Var edge_var = outer->getEdgeVar(edgeID);
                    if (outer->value(edge_var) == l_Undef) {
                        to_decide.push(mkLit(edge_var, true));
                    }
                }
            }
        }

        for (int i = over_history_qhead; i < g_over.historySize(); i++) {
            int edgeid = g_over.getChange(i).id;
            if (g_over.getChange(i).addition && g_over.edgeEnabled(edgeid)) {

            } else if (!g_over.getChange(i).addition && !g_over.edgeEnabled(edgeid)) {
                if(!path_is_cut) {
                    if (path_edges.has(edgeid)) {
                        return true;
                    }
                }else{
                    Var edge_var = outer->getEdgeVar(edgeid);
                    if (outer->value(edge_var) == l_Undef) {
                        to_decide.push(mkLit(edge_var, true));
                    }
                }
            }
        }

        for (int i = under_history_qhead; i < g_under.historySize(); i++) {
            int edgeid = g_under.getChange(i).id;
            if (path_edges.has(edgeid)) {
                if (g_under.getChange(i).addition && g_under.edgeEnabled(edgeid)) {
                    if (!path_is_cut) {
                        Var edge_var = outer->getEdgeVar(edgeid);
                        if (outer->value(edge_var) == l_Undef) {
                            to_decide.push(mkLit(edge_var, false));
                        }
                    }else {
                        if (path_edges.has(edgeid)) {
                            return true;
                        }
                    }
                } else if (!g_under.getChange(i).addition && !g_under.edgeEnabled(edgeid)) {

                }
            }
        }
        return false;
    }

    void drawGrid(DynamicGraph<Weight> & g, bool only_path=false){
        int width =10;
        int height = 10;

        vec<int> startsX;
        vec<int> endsX;
        vec<int> startsY;
        vec<int> endsY;

        vec<std::pair<int,int>> starts;
        vec<std::pair<int,int>> ends;


        bool found_source = false;
        bool found_dest = false;
        printf("\n");
        for(int y = 0;y<height;y++) {
            for (int x = 0; x < width; x++){
				int f_node = (y*height + x)*2+1;
                std::pair <int, int> p = std::make_pair(x,y);
                if (f_node == r->source || f_node-1==r->source){
                    //assert(starts.contains(p));
                    found_source=true;
                    printf("*");
                }else if (f_node == dest || f_node-1==dest){
                    found_dest=true;
                   // assert(ends.contains(p));
                    printf("@");
                }else{

                    if(starts.contains(p)){
                        int id = starts.indexOf(p)%10;
                        printf("%d",id);
                    }else if(ends.contains(p)){
                        //printf("%%");
                        int id = ends.indexOf(p)%10;
                        printf("%d",id);
                    }else {
                        printf("+");
                    }
                }
                if (x<width-1) {
                    int t_node = (y*height+x+1)*2;
                    //assert(g.hasEdge(f_node,t_node));
                    if(g.hasEdge(f_node, t_node) || g.hasEdge(t_node+1, f_node-1) || g.hasEdge(t_node, f_node) || g.hasEdge(f_node-1,t_node+1)){
                        if(only_path){
                            int edgea =    g.getEdge(f_node, t_node);
                            int edgeb =    g.getEdge(t_node, f_node);
                            if(path_edges.has(edgea) || path_edges.has(edgeb)){
                                printf("-");
                            }else{
                                printf(" ");
                            }
                        }else {
                            //int to_edge = g.getEdge(f_node,t_node);
                            //if(g.edgeEnabled(to_edge)){
                            printf("-");
                        }
                    }else{
                        printf(" ");
                    }
                }
            }
            printf("\n");
            for (int x = 0; x < width; x++) {
                int f_node = (y*height + x)*2+1;
                if (y < height - 1) {
                    int t_node = ((y + 1) * height + x)*2;
                    //assert(g.hasEdge(f_node, t_node));
                    if(only_path) {
                        int edgea = g.getEdge(f_node, t_node);
                        int edgeb = g.getEdge(t_node, f_node);

                        if (path_edges.has(edgea)) {
                            assert(g.hasEdge(edgea));
                            printf("| ");
                        }else if  (path_edges.has(edgeb)){
                            assert(g.hasEdge(edgeb));
                            printf("| ");
                        } else {
                            printf("  ");
                        }
                    }else if(g.hasEdge(f_node, t_node) || g.hasEdge(t_node+1, f_node-1) || g.hasEdge(t_node, f_node) || g.hasEdge(f_node-1,t_node+1)){
                        //int to_edge = g.getEdge(f_node, t_node);
                        //if(g.edgeEnabled(to_edge)){
                        //int to_edge = g.getEdge(f_node,t_node);
                        //if(g.edgeEnabled(to_edge)){
                        printf("| ");
                    }else{
                        printf("  ");
                    }
                }
            }
            printf("\n");
        }
        printf("\n");
        assert(found_source);
        assert(found_dest);
    }

    Lit decideTheory(CRef &decision_reason)override{
        if (outer->value(reach_lit)==l_Undef){
            return lit_Undef;//if the reach lit is unassigned, do not make any decisions here
        }



        if(needsRecompute()){
            r->stats_heuristic_recomputes++;
            computePath();
            if(opt_verb>=4 && (path_edges.size() || to_decide.size())) {

                printf("FlowGraph:\n");
	/*			for(int i = 0;i<flow_graph->edges();i++){
					int f = flow_graph->getEdge(i).from;
					int t = flow_graph->getEdge(i).to;
					if(flow_graph->edgeEnabled(i)){
						printf("%d: %d->%d\n",i,f,t);
					}
				}*/
                drawGrid(*flow_graph);
            }
            last_over_modification = g_over.modifications;
            last_over_deletion = g_over.deletions;
            last_over_addition = g_over.additions;
            over_history_qhead = g_over.historySize();
            last_over_history_clear = g_over.historyclears;

            last_under_modification = g_under.modifications;
            last_under_deletion = g_under.deletions;
            last_under_addition = g_under.additions;
            under_history_qhead = g_under.historySize();
            last_under_history_clear = g_under.historyclears;

        }
        for(Lit l:to_decide){
            assert(outer->value(l)!=l_False);
        }
        if(!path_is_cut){
            for(int edgeID:path_edges){
                assert(g_over.edgeEnabled(edgeID));
            }
        }else{
            for(int edgeID:path_edges){
                assert(!g_under.edgeEnabled(edgeID));
            }
        }


        if (to_decide.size()) {//  && last_decision_status == over_path->numUpdates() the numUpdates() monitoring strategy doesn't work if the constraints always force edges to be assigned false when other edges
            // are assigned true, as the over approx graph will always register as having been updated.
            //instead, check the history of the dynamic graph to see if any of the edges on the path have been assigned false, and only recompute in that case.
            while (to_decide.size()) {
                Lit l = to_decide.last();
                to_decide.pop();
                if (outer->value(l) == l_Undef) {
                    //stats_decide_time += rtime(2) - startdecidetime;
                    return l;
                }
            }
        }

        return lit_Undef;
    }
};

template<typename Weight>
void ReachDetector<Weight>::useFlowAsDecision(Lit outer_reach_lit, Lit outer_flow_literal, MaxflowDetector<Weight> * mf){


    if(!mf) {
        throw std::runtime_error("Undefined reach literal for flow heuristic");
    }
    int to = -1;
    for(int i = 0;i<reach_lits.size();i++){
        Lit l = reach_lits[i];
        if(l !=lit_Undef && var(outer->toSolver(l))==var(outer_reach_lit)){
            assert(reach_heuristics[i]);
			to = i;
            ReachHeuristic<Weight> * h = (ReachHeuristic<Weight>*) reach_heuristics[i];
            assert(h);

            Heuristic * f = new FlowPathHeuristic<Weight>(outer, this,mf,l, outer_flow_literal, to);
            h->setSubHeuristic(f);
			break;
        }
    }
    if(to<0){
        throw std::runtime_error("Undefined reach literal for flow heuristic");
    }



}

//Return the path (in terms of nodes)
template<typename Weight>
bool ReachDetector<Weight>::getModel_Path(int node, std::vector<int> & store_path){
	store_path.clear();
	 Reach & d = *underapprox_path_detector;
	 d.update();
	 if(!d.connected(node))
		 return false;
	 int u = node;
	 int p;
	while ((p = d.previous(u)) != -1) {
		Edge & edg = outer->edge_list[d.incomingEdge(u)]; //outer->edges[p][u];
		Var e = edg.v;
		lbool val = outer->value(e);
		assert(outer->value(e)==l_True);
		store_path.push_back(u);
		u = p;
	}
	assert(u==this->source);
	store_path.push_back(u);
	std::reverse(store_path.begin(),store_path.end());
	return true;
 }

template<typename Weight>
bool ReachDetector<Weight>::getModel_PathByEdgeLit(int node, std::vector<Lit> & store_path){
	store_path.clear();
	 Reach & d = *underapprox_path_detector;
	 d.update();
	 if(!d.connected(node))
		 return false;
	 int u = node;
	 int p;
	while ((p = d.previous(u)) != -1) {
		Edge & edg = outer->edge_list[d.incomingEdge(u)]; //outer->edges[p][u];
		Var e = edg.v;
		assert(outer->value(e)==l_True);
		lbool val = outer->value(e);
		assert(outer->value(e)==l_True);
		store_path.push_back(mkLit(outer->toSolver(e),false));
		u = p;
	}
	assert(u==this->source);
	std::reverse(store_path.begin(),store_path.end());
	return true;
 }

template class Monosat::ReachDetector<int> ;
template class Monosat::ReachDetector<int64_t> ;
template class Monosat::ReachDetector<double> ;
#include <gmpxx.h>
template class Monosat::ReachDetector<mpq_class> ;
