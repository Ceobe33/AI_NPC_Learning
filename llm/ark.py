import os
from openai import OpenAI

# 从环境变量中获取您的API KEY，配置方法见：https://www.volcengine.com/docs/82379/1399008
api_key = os.getenv('ARK_API_KEY')

client = OpenAI(
    base_url="https://ark.cn-beijing.volces.com/api/v3",
    api_key=api_key,
)

# response = client.responses.create(
response = client.chat.completions.create(
    # model="doubao-seed-2-1-pro-260628",
    model="doubao-seed-2-0-lite-260428",
    # input=[
    messages=[
        {
            "role": "user",
            "content": [

                {
                    # "type": "input_image",
                    "type": "image_url",
                    "image_url": "https://ark-project.tos-cn-beijing.volces.com/doc_image/ark_demo_img_1.png"
                },
                {
                    # "type": "input_text",
                    "type": "text",
                    "text": "你看见了什么？"
                },
            ],
        }
    ]
)

# print(response)
print(response.choices[0])
