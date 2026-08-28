from typing import TypedDict, List, Annotated
from langgraph.graph import StateGraph, END
from langchain_openai import ChatOpenAI
from langchain_core.messages import HumanMessage, SystemMessage
import operator

# ─── 状态定义 ───
class PlanExecuteState(TypedDict):
    task: str                           # 原始任务
    plan: List[str]                     # 规划器输出的子任务列表
    current_step: int                   # 当前执行步骤索引
    results: Annotated[List[str], operator.add]  # 各步骤执行结果
    final_output: str                   # 最终汇总输出

# ─── 模型初始化 ───
planner_llm  = ChatOpenAI(model="gpt-4o",      temperature=0)   # 规划器用强力模型
executor_llm = ChatOpenAI(model="gpt-4o-mini", temperature=0)   # 执行器用快速模型

# ─── 规划器节点 ───
def planner_node(state: PlanExecuteState) -> dict:
    """接受任务，输出子任务列表"""
    task = state["task"]
    
    response = planner_llm.invoke([
        SystemMessage(content="""你是一个专业的任务规划师。
将用户任务分解为 3-6 个具体、可执行的子任务。
每个子任务应该是独立的、可以用搜索或生成来完成的。
直接输出子任务列表，每行一个，用数字编号，不需要额外说明。"""),
        HumanMessage(content=f"任务：{task}")
    ])
    
    # 解析子任务列表
    lines = response.content.strip().split('\n')
    plan = []
    for line in lines:
        line = line.strip()
        if line and (line[0].isdigit() or line.startswith('-')):
            # 去掉编号前缀
            step = line.lstrip('0123456789.-） ).').strip()
            if step:
                plan.append(step)
    
    print(f"\n📋 规划器输出 {len(plan)} 个子任务：")
    for i, step in enumerate(plan, 1):
        print(f"  Step {i}: {step}")
    
    return {"plan": plan, "current_step": 0}

# ─── 执行器节点 ───
def executor_node(state: PlanExecuteState) -> dict:
    """执行当前子任务"""
    step_idx = state["current_step"]
    step = state["plan"][step_idx]
    task = state["task"]
    
    # 把之前的执行结果作为上下文
    prev_context = ""
    if state["results"]:
        prev_context = "\n\n已完成的步骤结果：\n" + "\n".join(
            f"- {r}" for r in state["results"]
        )
    
    response = executor_llm.invoke([
        SystemMessage(content=f"""你正在协助完成一个复杂任务，现在执行其中一个子步骤。
总体任务：{task}{prev_context}"""),
        HumanMessage(content=f"请执行这个子任务并给出结果（100字以内）：{step}")
    ])
    
    result = f"[Step {step_idx+1}] {step}: {response.content.strip()}"
    print(f"\n✅ {result}")
    
    return {
        "results": [result],
        "current_step": step_idx + 1
    }

# ─── 汇总节点 ───
def synthesizer_node(state: PlanExecuteState) -> dict:
    """汇总所有子任务结果，生成最终输出"""
    task = state["task"]
    results_text = "\n".join(state["results"])
    
    response = planner_llm.invoke([
        SystemMessage(content="你是一个专业的报告撰写者，将分步骤的调研结果整合成连贯的最终输出。"),
        HumanMessage(content=f"""
原始任务：{task}

各步骤执行结果：
{results_text}

请将以上结果整合成一份清晰、完整的最终输出（300字以内）。
""")
    ])
    
    return {"final_output": response.content.strip()}

# ─── 条件判断 ───
def should_continue(state: PlanExecuteState) -> str:
    """判断是继续执行还是汇总"""
    if state["current_step"] >= len(state["plan"]):
        return "synthesize"
    return "execute"

# ─── 构建图 ───
workflow = StateGraph(PlanExecuteState)

workflow.add_node("planner",     planner_node)
workflow.add_node("executor",    executor_node)
workflow.add_node("synthesizer", synthesizer_node)

workflow.set_entry_point("planner")
workflow.add_edge("planner", "executor")
workflow.add_conditional_edges(
    "executor",
    should_continue,
    {"execute": "executor", "synthesize": "synthesizer"}
)
workflow.add_edge("synthesizer", END)

app = workflow.compile()

# ─── 运行 ───
if __name__ == "__main__":
    task = "帮我制定一份从上海出发的云南大理5日旅行计划，预算8000元，偏好自然风景和当地美食"
    
    print(f"🎯 任务：{task}\n")
    
    result = app.invoke({
        "task": task,
        "plan": [],
        "current_step": 0,
        "results": [],
        "final_output": ""
    })
    
    print(f"\n{'='*50}")
    print("📄 最终输出：")
    print(result["final_output"])
