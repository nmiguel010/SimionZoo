#ifdef _WIN64

#include "DQN.h"
#include "SimGod.h"
#include "worlds\world.h"
#include "app.h"
#include "featuremap.h"
#include "features.h"
#include "noise.h"
#include "deep-vfa-policy.h"
#include "parameters-numeric.h"
#include "parameters.h"
#include "../tools/CNTKWrapper/CNTKWrapper.h"
#include <algorithm>
#include "deep-vfa-policy.h"
#include "config.h"

DQN::~DQN()
{
	//We need to manually call the NN_DEFINITION destructor
	m_pNNDefinition->destroy();
	if (m_pTargetQNetwork) m_pTargetQNetwork->destroy();
	if (m_pOnlineQNetwork) m_pOnlineQNetwork->destroy();
	if (m_pMinibatch) m_pMinibatch->destroy();
	CNTKWrapperLoader::UnLoad();
}

DQN::DQN(ConfigNode* pConfigNode)
{
	m_inputState = MULTI_VALUE_VARIABLE<STATE_VARIABLE>(pConfigNode, "Input-State", "Set of variables used as input of the QNetwork");
	m_numActionSteps = INT_PARAM(pConfigNode, "Num-Action-Steps", "Number of discrete values used for the output action", 2);
	m_outputAction = ACTION_VARIABLE(pConfigNode, "Output-Action", "The output action variable");
	m_learningRate = DOUBLE_PARAM(pConfigNode, "Learning-Rate", "The learning rate at which the agent learns", 0.000001);

	//The wrapper must be initialized before loading the NN definition
	CNTKWrapperLoader::Load();
	m_policy = CHILD_OBJECT_FACTORY<DiscreteDeepPolicy>(pConfigNode, "Policy", "The policy");
	m_pNNDefinition = NN_DEFINITION(pConfigNode, "neural-network", "Neural Network Architecture");
}

void DQN::deferredLoadStep()
{
	//we defer all the heavy-weight initializing stuff and anything that depends on the SimGod
	m_Q_s_p = vector<double>(m_numActionSteps.get());
	m_Q_s = vector<double>(m_numActionSteps.get());

	Descriptor& actionDescr = SimionApp::get()->pWorld->getDynamicModel()->getActionDescriptor();
	
	//set the input-outputs
	for (size_t stateVarIndex = 0; stateVarIndex < m_inputState.size(); stateVarIndex++)
		m_pNNDefinition->addInputStateVar(stateVarIndex);
	m_pNNDefinition->setDiscretizedActionVectorOutput(m_outputAction.get(), m_numActionSteps.get()
		, actionDescr[m_outputAction.get()].getMin(), actionDescr[m_outputAction.get()].getMax());

	//create the networks
	m_pOnlineQNetwork = m_pNNDefinition->createNetwork(m_learningRate.get());
	m_pTargetQNetwork = m_pOnlineQNetwork->clone();

	//create the minibatch
	//The size of the minibatch is the experience replay update size
	//This is because we only perform updates in replaying-experience mode
	size_t minibatchSize = SimionApp::get()->pSimGod->getExperienceReplayUpdateSize();
	if (minibatchSize == 0)
		Logger::logMessage(MessageType::Error, "Both DQN and Double-DQN require the use of the Experience Replay Buffer technique");
	m_pMinibatch = m_pNNDefinition->createMinibatch(minibatchSize);
}

double DQN::selectAction(const State * s, Action * a)
{
	m_pOnlineQNetwork->evaluate(s, a, m_Q_s);

	size_t selectedAction = m_policy->selectAction(m_Q_s);

	a->set(m_outputAction.get()
		, m_pNNDefinition->getActionIndexOutput(selectedAction));

	return 1.0;
}

INetwork* DQN::getQNetworkForTargetActionSelection()
{
	return m_pTargetQNetwork;
}

double DQN::update(const State * s, const Action * a, const State * s_p, double r, double behaviorProb)
{
	if (SimionApp::get()->pSimGod->bReplayingExperience())
	{
		double gamma = SimionApp::get()->pSimGod->getGamma();

		//get Q(s_p) for the current tuple (target/online-weights)
		getQNetworkForTargetActionSelection()->evaluate(s_p, a, m_Q_s_p);

		//calculate argmaxQ(s_p)
		size_t argmaxQ = distance(m_Q_s_p.begin(), max_element(m_Q_s_p.begin(), m_Q_s_p.end()));

		//estimate Q(s_p, argMaxQ; target-weights or online-weights)
		//THIS is the only real difference between DQN and Double-DQN
		//We do the prediction step again only if using Double-DQN (the prediction network
		//will be different to the online network)
		if (getQNetworkForTargetActionSelection() != m_pTargetQNetwork)
			m_pTargetQNetwork->evaluate(s_p, a, m_Q_s_p);

		//calculate targetvalue= r + gamma*Q(s_p,a)
		double targetValue = r + gamma * m_Q_s_p[argmaxQ];

		//get the current value of Q(s)
		m_pOnlineQNetwork->evaluate(s, a, m_Q_s);

		//change the target value only for the selecte action, the rest remain the same
		//store the index of the action taken
		size_t selectedActionId =
			m_pNNDefinition->getClosestOutputIndex(a->get(m_outputAction.get()));
		m_Q_s[selectedActionId] = targetValue;

		m_pMinibatch->addTuple(s, a, m_Q_s);
	}
	//We only train the network in direct-experience updates to simplify mini-batching
	else if (m_pMinibatch->isFull())
	{
		SimGod* pSimGod = SimionApp::get()->pSimGod.ptr();

		//update the network finally
		m_pOnlineQNetwork->train(m_pMinibatch);

		//update the prediction network
		if (pSimGod->bUpdateFrozenWeightsNow())
		{
			if (m_pTargetQNetwork)
				m_pTargetQNetwork->destroy();
			m_pTargetQNetwork = m_pOnlineQNetwork->clone();
		}
	}
	return 1.0; //TODO: Estimate the TD-error??
}


DoubleDQN::DoubleDQN(ConfigNode* pParameters): DQN (pParameters)
{}

INetwork* DoubleDQN::getQNetworkForTargetActionSelection()
{
	return m_pOnlineQNetwork;
}

#endif