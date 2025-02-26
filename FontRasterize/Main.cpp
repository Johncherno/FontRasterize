#include"Fontbuilder\ui_opengl.h"
static void checkGLError(const char* stmt, const char* fname, int line) {
    GLenum err = glGetError();
    while (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error " << err << " at " << fname << ":" << line << " for " << stmt << std::endl;
        err = glGetError();
    }
}
#define gl_check(stmt) do { stmt; checkGLError(#stmt, __FILE__, __LINE__); } while (0)
// addtext������λ�õߵ�һ��
int main()
{
      //������һ������
      GLFWwindow* pwindow;//����ָ��
      if (!glfwInit())
      return -1;
      
      pwindow = glfwCreateWindow(900, 800, "fontRasterizer", NULL, NULL);//��ʼ������
      glfwMakeContextCurrent(pwindow);//�ڴ����ϻ�����
      glewInit();//glew��ʼ��
      glViewport(0, 0, 900, 800);//��Ұ��С
   
     
  
      
      SetUpContextIO();
      BuiltIO* IO = GetIo();
      IO->atlas.AddFont_FromFile("C:\\Windows\\Fonts\\simsun.ttc", 18, IO->atlas.Get_ChineseGlyph_CodePointRange());//������������
      BuildBackendData();//�����������ݶ������� ���������ַ�ͼ�� ��������ɫ��

      
       bool WindowShow = false;
       UI* ui = new UI;//ʵ����UI������
      while (!glfwWindowShouldClose(pwindow))//��Ϸѭ��
      {
          
         
          glClear(GL_COLOR_BUFFER_BIT);//�����ɫ ��Ȼ���
          glClearColor(0.0, 0.0, 0.0, 0.0);
          UpdateEvent(pwindow);//��Ӧ�����¼�
           ui->StartShowWindow(u8"First��������X", true);
          
           ui->StartMenu( HorizontalShow);
           ui->Menu(u8"First�˵���", true);
           ui->MenuItem(u8"First�˵�������Ŀ1", nullptr);
           ui->MenuItem(u8"First�˵�������Ŀ2", &WindowShow);
           ui->MenuItem(u8"First�˵�������Ŀ3", nullptr);
           ui->EndMenu();

           ui->EndShowWindow();//������ǰ������Ҫ��һ�ε���ʱ����һ�����ڵ�λ��
          


          ui->StartShowWindow(u8"Second��������", WindowShow);
          ui->BgText(u8"�������йܴ���򱾻�������Ϊ DLL ��д����Ӧ�á� \n"
              "�������Ӧ�õ����й� DLL����������Ҫ�������ߣ�\n"
              "���������Ŀ������ͬʱ�����йܺͱ�����������\n" 
             " ȷ������ȡ������Ҫ�� DLL ��Ŀ���Ǵӵ���Ӧ����Ŀ��ʼ���ԡ� �й���ϸ��Ϣ���������Σ��ڻ��ģʽ�µ��ԡ�\n");

         
          ui->EndShowWindow();

         
          RenderDrawData(ui);//����UI��ӵĻ�������
   
        
         ClearEvent();//����¼���Ϣ
      	glfwSwapBuffers(pwindow);
      	glfwPollEvents();//��ʼ��Ӧ���� ���������Ϣ
      
        
      }
      DestructResource();//�ͷŶ��㻺��
      glfwTerminate();//�ս������
      return 0;
}
//mousePos = IO->InputMessage.MousePos;
//
//if (IO->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT])
//{
//    Xoffset = IO->InputMessage.MouseDeltaOffset.x;
//    Yoffset = IO->InputMessage.MouseDeltaOffset.y;
//
//
//}
//scroll = IO->InputMessage.Scroll;
//if (IO->InputMessage.KeyMessage.key == GLFW_KEY_LEFT_CONTROL && IO->InputMessage.KeyMessage.press)
//{
//    scalex += scroll * 0.05f; // �������������ƴ�С�ı仯����
//    scaley += scroll * 0.05f;
//}
//if (scalex < 0.1f) scalex = 0.1f;
//if (scalex > 3.0f) scalex = 3.0f;
//
//if (scaley < 0.1f) scaley = 0.1f;
//if (scaley > 3.0f) scaley = 3.0f;
////glm::vec2 secondMenuPos = glm::vec2(0,0);
//
//
//TextClipR.Min = BeginPos;
//TextClipR.Max = BeginPos + glm::vec2(980, 860);
//
//scrollBarCol = glm::vec3(1, 1, 1);
//if (TextRectMove && TextClipR.Contain(mousePos))
//{
//    BeginPos.x += Xoffset;
//    BeginPos.y += Yoffset;
//
//}
//if (TextClipR.Contain(mousePos))
//{
//    textPos.y += scroll;
//    scrollRect.Min.y -= scroll;
//    scroll = 0;
//}
//if (scrollRect.Contain(mousePos - BeginPos))
//{
//    scrollBarMove = true;
//    TextRectMove = false;
//    scrollBarCol = glm::vec3(0, 1, 0);
//}
//else
//{
//    scrollBarMove = false;
//    TextRectMove = true;
//}
//if (scrollBarMove)
//{
//    scrollRect.Min.y += 3 * Yoffset;
//    textPos.y -= 3 * Yoffset;
//
//}
//scrollRect.Max = scrollRect.Min + glm::vec2(20, 140);
//
//IO->drawdata.drawlist->AddRectFill(TextClipR.Min, TextClipR.Max, glm::vec3(0, 0, 1), 9.0f, 1);
//IO->drawdata.drawlist->AddRectFill(BeginPos + scrollRect.Min, BeginPos + scrollRect.Max, scrollBarCol, 9.0f, 1);//������
//IO->drawdata.drawlist->AddClipText(textPos, glm::vec3(1, 0, 0), TestText, TextClipR, scalex, scaley);

// Ȼ��ʹ��projectedMouse���ת�����������к�����������������ı���
       // std::cout << "projectedMouse.x" << projectedMouse.x << " " << "projectedMouse.y" << projectedMouse.y << std::endl;
       // io.drawdata.drawlist->AddText(BeginPos, glm::vec3(1, 1, 1), u8"��Ӧ���ı�����", &io.GetfontAtlas(), scalex, scaley);
        //io.drawdata.drawlist->AddRectFill(BeginPos-glm::vec2(0,4), BeginPos - glm::vec2(0, 4) + glm::vec2(164, 12), firstCol, 5.0f, scalex, &io.GetfontAtlas());
        //io.drawdata.drawlist->AddText(BeginPos, glm::vec3(1, 1, 1), u8"������ŵ�����", &io.GetfontAtlas(), scalex, scaley);
        //secondMenuPos = BeginPos + MenuOffset;
        //io.drawdata.drawlist->AddRectFill(secondMenuPos - glm::vec2(0, 4), secondMenuPos - glm::vec2(0, 4) + glm::vec2(124, 12), secondCol, 5.0f, scalex, &io.GetfontAtlas());
        //io.drawdata.drawlist->AddText(secondMenuPos, glm::vec3(1, 1, 1), u8"���ص�����", &io.GetfontAtlas(), scalex, scaley);

        //if (mousePos.x >= BeginPos.x && mousePos.y >= BeginPos.y && mousePos.x <= (BeginPos.x + 261) && mousePos.y <= (BeginPos.y + 71))//&& mousePos.x <= BeginPos.x + 31 && mousePos.y <= BeginPos.y + 31
        //{
        //    firstCol = glm::vec3(0, 1, 1);
        //    shouldDrawText = true;

        //}
        //else
        //{
        //    firstCol = glm::vec3(1, 0, 0);
        //    shouldDrawText = false;
        //}
        //if (shouldDrawText)
        //{
        //    //std::cout << "��������" << std::endl;
        //    io.drawdata.drawlist->AddRectFill(BeginPos+glm::vec2(0,5+io.GetfontAtlas().fontSize * scaley), BeginPos + glm::vec2(334,470),glm::vec3(0,0,1), 5.0f, scalex, &io.GetfontAtlas());
        //    io.drawdata.drawlist->AddText(BeginPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley), glm::vec3(1, 0, 1), u8"��ֻ�ں���", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(BeginPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 2), glm::vec3(0, 1, 1), u8"ƫƫϲ����", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(BeginPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 3), glm::vec3(1, 0, 1), u8"�ɶ�", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(BeginPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 4), glm::vec3(1, 1, 0), u8"������", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(BeginPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 5), glm::vec3(1,0, 1), u8"�����Ļ�", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(BeginPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 6), glm::vec3(1, 1, 0), u8"Ͷ��״ The Oath", &io.GetfontAtlas(), scalex, scaley);
        //   

        //}

        //if (mousePos.x >= secondMenuPos.x && mousePos.y >= secondMenuPos.y && mousePos.x <= (secondMenuPos.x + 261) && mousePos.y <= (secondMenuPos.y +71))
        //{
        //    secondCol= glm::vec3(0, 1, 1);
        //    io.drawdata.drawlist->AddRectFill(secondMenuPos + glm::vec2(0, 5 + io.GetfontAtlas().fontSize * scaley), secondMenuPos + glm::vec2(334, 470), glm::vec3(0, 0, 1), 5.0f, scalex, &io.GetfontAtlas());
        //    io.drawdata.drawlist->AddText(secondMenuPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley), glm::vec3(1, 0, 1), u8"Perfect", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(secondMenuPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 2), glm::vec3(0, 1, 1), u8"Love Story ", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(secondMenuPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 3), glm::vec3(1, 0, 1), u8"A Thousand Years", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(secondMenuPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 4), glm::vec3(1, 1, 0), u8"500 Miles Away From Home", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(secondMenuPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 5), glm::vec3(1, 0, 1), u8"Vincent", &io.GetfontAtlas(), scalex, scaley);
        //    io.drawdata.drawlist->AddText(secondMenuPos + glm::vec2(3, 7 + io.GetfontAtlas().fontSize * scaley * 6), glm::vec3(1, 1, 0), u8"Can't Help Falling in Love", &io.GetfontAtlas(), scalex, scaley);

        //}
        //else
        //{
        //    secondCol = glm::vec3(1, 1, 0);
        //}
        // io.drawlist()->AddTime(BeginPos + glm::vec2(115.0f, 98.0f), glm::vec3(1, 0, 1), &io.GetfontAtlas(), scalex * 7, scaley * 7);
        //io.drawlist()->AddNumber(BeginPos + glm::vec2(115.0f, 58.0f), glm::vec3(1, 0, 0), time, 5, &io.GetfontAtlas(), scalex * 6, scaley * 6);
         //io.drawlist()->AddRectFill(BeginPos + glm::vec2(-25.0f, 108.0f), BeginPos + glm::vec2(125.0f, 138.0f), glm::vec3(1, 0, 1), 8,scalex, &io.GetfontAtlas());


        //io.drawlist()->AddRectFill(BeginPos + glm::vec2(-15, -16) + glm::vec2(-5.0f, 108.0f), BeginPos +glm::vec2(0, -56) + glm::vec2(125.0f, 138.0f), glm::vec3(1, 1, 1), 8, scalex, &io.GetfontAtlas());
        //io.drawlist()->AddText(BeginPos + glm::vec2(-15, -16) + glm::vec2(-5.0f, 118.0f), glm::vec3(1, 1, 1), u8"������ŵ�����", &io.GetfontAtlas(), scalex, scaley);

        //io.drawlist()->AddRectFill(BeginPos + glm::vec2(-15, -56*2) + glm::vec2(-5.0f, 108.0f), BeginPos + glm::vec2(0, -56*2) + glm::vec2(125.0f, 138.0f), glm::vec3(0, 0, 1), 8, scalex, &io.GetfontAtlas());
       // io.drawdata.drawlist->AddText(BeginPos + glm::vec2(10,12) + glm::vec2(-5.0f, 118.0f), glm::vec3(1,0, 1), u8"���ص�����", &io.GetfontAtlas(), scalex, scaley);
       // io.drawdata.drawlist->RendeAllCharactersIntoScreen(BeginPos + glm::vec2(0, 43), 1000, glm::vec3(1, 0, 1), &io.GetfontAtlas(), scalex, scaley);

        //io.drawlist()->AddRotText(BeginPos + glm::vec2(0, -56), glm::vec3(1, 0, 0), inputstr, &io.GetfontAtlas(), scalex * 3, scaley * 3, time);
       // io.drawlist()->AddRotText(BeginPos + glm::vec2(12, -56*4), glm::vec3(1, 0, 0), inputstr2, &io.GetfontAtlas(), scalex * 3, scaley * 4, time);
//bool        Contains(const ImVec2& p) const { return p.x >= Min.x && p.y >= Min.y && p.x < Max.x && p.y < Max.y; } ������λ���Ƿ��ھ��η�����
//static float time = 0.0f;
//Drawinglist.AddRectFill(pos + glm::vec2(-5.0f, 98.0f), pos + glm::vec2(55.0f, 118.0f), glm::vec3(1, 1, 1), 5, &atlas);
//Drawinglist.AddNumber(pos + glm::vec2(-5.0f, 58.0f), glm::vec3(1, 1, 0), time, 2, &atlas, scalex * 3, scaley * 3);//��time��ʾ
////Drawinglist.AddRotText(pos + glm::vec2(-5.0f, 18.0f), glm::vec3(0, sin(0.3 * time), 1), text, &atlas, scalex * 3, scaley * 3, time);
////Drawinglist.AddTestFont(pos,2300,2000,glm::vec2(0,1),glm::vec2(1,0),glm::vec3(1,0,0));
//Drawinglist.RendeAllCharactersIntoScreen(pos, 300, col, &atlas, scalex, scaley);
//time += 0.003f;

//static void CheckGLError(const char* stmt, const char* fname, int line) {
//    GLenum err = glGetError();
//    while (err != GL_NO_ERROR) {
//        std::cerr << "OpenGL error " << err << " at " << fname << ":" << line << " for " << stmt << std::endl;
//        err = glGetError();
//    }
//}
//#define GL_CHECK(stmt) do { stmt; CheckGLError(#stmt, __FILE__, __LINE__); } while (0)
//class TEST
//{
//  public:
//     std::vector<float> vertices;
//      void init()
//      {
//          GL_CHECK(glGenVertexArrays(1, &VAO1));
//          GL_CHECK(glGenBuffers(1, &VBO1));
//          SetUpBufferState(VAO1, VBO1);
//      }
//      void setVTXbufferstate()
//      {
//          GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, VBO1));
//          GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW));
//      }
//      void DrawCmd()
//      {
//          GL_CHECK(glBindVertexArray(VAO1));
//          glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size() / 5);
//          GL_CHECK(glBindVertexArray(0));
//
//      }
//  private:
//      void SetUpBufferState(GLuint vertex_object_array, GLuint vertex_Buffer_object)
//      {
//          GL_CHECK(glBindVertexArray(vertex_object_array));
//          GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vertex_Buffer_object));
//          //GL_CHECK(glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW)); // ��ʼ��Ϊ 0 ��С���Ժ����
//
//          GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0));
//          GL_CHECK(glEnableVertexAttribArray(0));
//
//          GL_CHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float))));
//          GL_CHECK(glEnableVertexAttribArray(1));
//
//          GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
//          GL_CHECK(glBindVertexArray(0));
//      }
//      GLuint VAO1, VBO1;
//};
//
//// Vertex Shader Source Code
//const char* vertexShaderSource = R"(
//#version 330 core
//
//layout (location = 0) in vec2 aPos;   // ���붥��λ�� (attribute 0)
//layout (location = 1) in vec3 aColor; // ���붥����ɫ (attribute 1)
//
//out vec3 vertexColor; // ��Ƭ����ɫ�����ݵ���ɫ
//
//void main()
//{
//    gl_Position = vec4(aPos, 0.0, 1.0); // ����ά����ת��Ϊ��ά�������
//    vertexColor = aColor;              // ��������ɫ���ݸ�Ƭ����ɫ��
//}
//)";
//
//// Fragment Shader Source Code
//const char* fragmentShaderSource = R"(
//#version 330 core
//
//in vec3 vertexColor;    // �Ӷ�����ɫ�����յ���ɫ
//out vec4 FragColor;     // �����Ƭ����ɫ
//
//void main()
//{
//    FragColor = vec4(vertexColor, 1.0); // ����� alpha ֵ����ɫ
//}
//)";
//
//int main()
//{
//    // Initialize GLFW
//    if (!glfwInit())
//    {
//        std::cerr << "Failed to initialize GLFW" << std::endl;
//        return -1;
//    }
//
//    // Configure GLFW
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//
//    // Create a window
//    GLFWwindow* window = glfwCreateWindow(800, 600, "Dynamic Vertex Update", NULL, NULL);
//    if (!window)
//    {
//        std::cerr << "Failed to create GLFW window" << std::endl;
//        glfwTerminate();
//        return -1;
//    }
//
//    glfwMakeContextCurrent(window);
//
//    // Initialize GLEW
//    if (glewInit() != GLEW_OK)
//    {
//        std::cerr << "Failed to initialize GLEW" << std::endl;
//        return -1;
//    }
//
//    // Set viewport
//    glViewport(0, 0, 800, 600);
//
//    TEST test;
//    test.init();
//    
//
//    // Create, compile and attach vertex shader
//    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
//    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
//    glCompileShader(vertexShader);
//
//    // Create, compile and attach fragment shader
//    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
//    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
//    glCompileShader(fragmentShader);
//
//    // Link shaders into a program
//    GLuint shaderProgram = glCreateProgram();
//    glAttachShader(shaderProgram, vertexShader);
//    glAttachShader(shaderProgram, fragmentShader);
//    glLinkProgram(shaderProgram);
//
//    // Delete shaders as they're no longer needed
//    glDeleteShader(vertexShader);
//    glDeleteShader(fragmentShader);
//
//  
//
//    // Render loop
//    while (!glfwWindowShouldClose(window))
//    {
//        // Process input
//        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
//            glfwSetWindowShouldClose(window, true);
//
//        // Clear screen
//        glClear(GL_COLOR_BUFFER_BIT);
//
//        // Generate dynamic vertex data
//       test.vertices.clear(); // Clear vector for new data
//        float time = glfwGetTime();
//        test.vertices.push_back(0.5f * sin(time));  test.vertices.push_back(0.5f * cos(time)); // Dynamic point
//        test.vertices.push_back(1.0f);  test.vertices.push_back(0.0f);  test.vertices.push_back(0.0f); // Color Red
//
//        test.vertices.push_back(-0.5f);  test.vertices.push_back(0.5f); // Fixed point
//        test.vertices.push_back(0.0f);  test.vertices.push_back(1.0f);  test.vertices.push_back(0.0f); // Color Green
//
//        test.vertices.push_back(0.5f);  test.vertices.push_back(-0.5f); // Fixed point
//        test.vertices.push_back(0.0f);  test.vertices.push_back(0.0f);  test.vertices.push_back(1.0f); // Color Blue
//
//        test.vertices.push_back(-0.5f);  test.vertices.push_back(-0.5f); // Fixed point
//        test.vertices.push_back(1.0f); test.vertices.push_back(1.0f);  test.vertices.push_back(0.0f); // Color Yellow
//
//        // Update VBO data
//      /*  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, VBO));
//        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW));*/
//        test.setVTXbufferstate();
//
//        // Use shader program
//        glUseProgram(shaderProgram);
//
//        // Render quad
//       /* GL_CHECK(glBindVertexArray(VAO));
//        glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size() / 5);
//        GL_CHECK(glBindVertexArray(0));*/
//        test.DrawCmd();
//
//
//        // Swap buffers and poll events
//        glfwSwapBuffers(window);
//        glfwPollEvents();
//    }
//
//    //// Cleanup
//    //glDeleteVertexArrays(1, &VAO);
//    //glDeleteBuffers(1, &VBO);
//    //glDeleteProgram(shaderProgram);
//
//    // Terminate GLFW
//    glfwTerminate();
//    return 0;
//}