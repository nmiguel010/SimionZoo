// CNTK-Test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#ifdef _WIN64

#include <iostream>
#include "CNTKLibrary.h"
#include <unordered_map>
using namespace std;
using namespace CNTK;

void PrintParameters(FunctionPtr function)
{
	for (size_t i = 0; i < function->Parameters().size(); i++)
	{
		vector<double> cpuVector = vector<double>(function->Parameters()[i].Shape().TotalSize());
		NDArrayViewPtr cpuParameterArray = MakeSharedObject<NDArrayView>(function->Parameters()[i].Shape(), cpuVector);
		cpuParameterArray->CopyFrom(*(function->Parameters()[i]).Value());
		for (int j = 0; j < cpuVector.size(); j++)
			std::wcout << L"Parameter#" << i << L": " << cpuVector[j] << L"\n";
	}
	std::wcout << L"\n";
}

void PrintParameters(const vector<Parameter>& parameters)
{
	for (size_t i = 0; i < parameters.size(); i++)
	{
		vector<double> cpuVector = vector<double>(parameters[i].Shape().TotalSize());
		NDArrayViewPtr cpuParameterArray = MakeSharedObject<NDArrayView>(parameters[i].Shape(), cpuVector);
		cpuParameterArray->CopyFrom(*(parameters[i].Value()));
		for (int j = 0; j < cpuVector.size(); j++)
			std::wcout << L"Parameter " << parameters[i].Name() << L" : " << cpuVector[j] << L"\n";
	}
	std::wcout << L"\n";
}

void PrintValuePtr(ValuePtr valuePtr)
{
	vector<double> cpuVector = vector<double>(valuePtr->Shape().TotalSize());
	NDArrayViewPtr cpuArrayView =
		MakeSharedObject<NDArrayView>(valuePtr->Shape(), cpuVector, false);
	cpuArrayView->CopyFrom(*valuePtr->Data());

	std::wcout << L"[ ";
	for (size_t i = 0; i<cpuVector.size(); i++)
		std::wcout << cpuVector[i] << L" ";
	std::wcout << L"]\n";
}



int main()
{

	//Play environment to help me understand what I'm doing with gradients and the other stuff
	//  -s: only a scalar
	//  -a: only a scalar
	//Q(s,a) = s*a		, meaning that, the greater the action is and the greater the state is, the greater the state-action value
	//pi(s) = w*s + b	,where 'w' and 'b' are the learnable parameters
	const size_t stateDim = 1;
	const size_t actionDim = 1;
	const size_t batchSize = 20;
	double s_0 = 2.0;
	double s_1 = 3.0;
	double a_0 = 1.0;
	double a_1 = -3.0;
	vector<double> batch_s = vector<double>(stateDim*batchSize);
	for (int i= 0; i<stateDim*batchSize; i++)
		batch_s[i]= (rand() / (double)RAND_MAX) * 10.0;

	//We should evaluate the actor to properly do DDPG update
	vector<double> batch_a = vector<double>(actionDim*batchSize);
	for (int i = 0; i<stateDim*batchSize; i++)
		batch_a[i] = (rand() / (double)RAND_MAX) * 20.0;

	const size_t outputDim = stateDim * actionDim;

	//Create a simple Q(s,a) network as a linear combination of s and a
	Variable Q_input_s = InputVariable({ stateDim }, DataType::Double, true, L"s");
	Variable Q_input_a = InputVariable({ actionDim }, DataType::Double, true, L"a");
	FunctionPtr Neg_a = ElementTimes(Q_input_a, Constant::Scalar(DataType::Double, -1.0));
	FunctionPtr QFunction = Plus(Q_input_s, Neg_a, L"Q(s,a)");
	
	//Create pi(s) network
	Variable Policy_input_s = InputVariable({ stateDim }, DataType::Double, true, L"s");
	Variable Policy_mult = Parameter({ stateDim }, DataType::Double, 10.0, DeviceDescriptor::UseDefaultDevice(),L"policy-mult");
	Variable Policy_bias = Parameter({ stateDim }, DataType::Double, 0.5, DeviceDescriptor::UseDefaultDevice(),L"policy-bias");
	FunctionPtr inputScale = ElementTimes(Policy_input_s, Policy_mult, L"scale");
	FunctionPtr Policy = Plus(Policy_bias, inputScale, L"pi(s)");

	//Initialize learner
	vector<double> learningRateSchedule = vector<double>(1);
	learningRateSchedule[0] = 0.001;
	vector<double> momentumSchedule = vector<double>(1);
	momentumSchedule[0] = 0.999;
	//LearnerPtr PolicyLearner = AdamLearner(Policy->Parameters(), LearningRatePerSampleSchedule(0.001), MomentumPerSampleSchedule(0.999));
	LearnerPtr PolicyLearner = AdamLearner(Policy->Parameters(), TrainingParameterPerSampleSchedule(learningRateSchedule), TrainingParameterPerSampleSchedule(momentumSchedule));



	//calculate Q's gradient wrt input action
	unordered_map<Variable, ValuePtr> qGradArgs= unordered_map<Variable, ValuePtr>();
	unordered_map<Variable, ValuePtr> qGradOutput = unordered_map<Variable, ValuePtr>();

	qGradArgs[Q_input_s] = Value::CreateBatch({ (size_t)stateDim }, batch_s, DeviceDescriptor::UseDefaultDevice());
	qGradArgs[Q_input_a] = Value::CreateBatch({ (size_t)actionDim }, batch_a, DeviceDescriptor::UseDefaultDevice());

	qGradOutput[Q_input_a] = nullptr;
	QFunction->Gradients(qGradArgs, qGradOutput);

	std::wcout << L"Q gradients:\n";
	PrintValuePtr(qGradOutput[Q_input_a]);
	assert(qGradOutput[Q_input_a]->Shape().TotalSize() == batchSize * actionDim);

	//Policy update
	std::wcout << L"### POLICY UPDATE\n";
	std::wcout << L"Forward pass...\n";
	unordered_map<Variable, ValuePtr> policyFwdArgs = {};
	unordered_map<Variable, ValuePtr> policyFwdOutput = {};
	policyFwdArgs[Policy_input_s] = Value::CreateBatch({ stateDim }, batch_s, DeviceDescriptor::UseDefaultDevice());
	policyFwdOutput[Policy] = nullptr;
	auto backPropState= Policy->Forward(policyFwdArgs, policyFwdOutput, DeviceDescriptor::UseDefaultDevice(), { Policy });
	
	//Copy the q gradient to a Cpu vector: qGradientCpuVector
	ValuePtr qGradient = qGradOutput[Q_input_a];
	vector<double> qGradientCpuVector = vector<double>(qGradient->Shape().TotalSize());
	NDArrayViewPtr qParameterGradientCpuArrayView =
		MakeSharedObject<NDArrayView>(qGradient->Shape(), qGradientCpuVector, false);
	qParameterGradientCpuArrayView->CopyFrom(*qGradient->Data());
	assert(qGradient->Shape().TotalSize() == actionDim*batchSize);

	vector<double> backwardPassRootValue = vector<double>(batchSize);
	auto PolicyRootGradientValue = MakeSharedObject<Value>(MakeSharedObject<NDArrayView>(
		Policy->Output().Shape().AppendShape({ 1, backwardPassRootValue.size() / Policy->Output().Shape().TotalSize() })
		, backwardPassRootValue, false));

	std::wcout << PolicyRootGradientValue->Shape().AsString() << L"\n";
	
	for (int i = 0; i < batchSize; i++)
		backwardPassRootValue[i] = -qGradientCpuVector[i];

	std::wcout << L"Backward pass...\n";
	unordered_map<Variable, ValuePtr> parameterGradients = {};
	for (const Parameter& parameter : PolicyLearner->Parameters())
		parameterGradients[parameter] = nullptr;
	Policy->Backward(backPropState, { {Policy, PolicyRootGradientValue } }, parameterGradients);

	std::wcout << L"Policy gradients:\n";
	for (const Parameter& parameter : PolicyLearner->Parameters())
		PrintValuePtr(parameterGradients[parameter]);
	
	std::wcout << L"Policy parameters:\n";
	PrintParameters(PolicyLearner->Parameters());

	//update parameters
	std::wcout << L"Update policy parameters: \n";
	std::unordered_map<Parameter, NDArrayViewPtr> gradients = {};
	for (const auto& parameter : PolicyLearner->Parameters())
		gradients[parameter] = parameterGradients[parameter]->Data();
	bool updated= PolicyLearner->Update(gradients, batchSize, false);

	std::wcout << L"Policy parameters:\n";
	PrintParameters(PolicyLearner->Parameters());

	std::wcout << L"\n\nPress any key...";
	char c= getchar();
    return 0;
}
#else

int main()
{
	return 0;
}

#endif