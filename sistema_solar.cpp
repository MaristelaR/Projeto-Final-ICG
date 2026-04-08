//definição das bibliotecas
#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PI 3.1415

//Configurações dos estados e da câmera
float tempo = 0.0f; //controla o avanço das orbitas e a animação em geral
bool pausado = false; //define se a animação esta parada ou não
bool mostrarOrbitas = false; //liga/desliga desenho das orbitas
float anguloCamera = 0.5f; 
float distanciaCamera = 450.0f;
float alturaCamera = 250.0f;
float anguloCameraManual = 0.0f; //armazena o deslocamento aplicado pelo usuário

//estrutura de dados para construir o "fundo do universo", as estrelas 
struct Estrela {
    float x, y, z,fase, brilho; // posições no mundo 3D, intensidade da luz e atraso de tempo
};
const int NUM_ESTRELAS = 3000; //quantidade de estrelas na cena
Estrela estrelas[NUM_ESTRELAS]; //reserva espaço na memória para guardar as estrelas
const float RAIO_ESTRELAS = 600.0f; //"limite do universo"

//Função para lidar com as texturas. Pega a imagem e transforma em textura
GLuint loadTexture(const char* nomeArquivo, bool fixarBorda = false) {
    int largura, altura, canais;
    stbi_set_flip_vertically_on_load(0); 
    unsigned char* dados = stbi_load(nomeArquivo, &largura, &altura, &canais, 4); //força os 4canais do rgba, mesmo que não tenha
    if (!dados) { printf("Erro ao carregar: %s\n", nomeArquivo); return 0; }

    GLuint idTextura; //pede o numero de identificação unico para cada textura
    glGenTextures(1, &idTextura);
    glBindTexture(GL_TEXTURE_2D, idTextura); //tudo que for configurado é para a estrutura da vez
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    //para não ficar serrilhado quando afasta, usa MIPmaps(versão menores da imagem) para manter suavidade
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); //GL_LINEAR suaviza quando chega perto
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    //faz a textura se repetir, pois a textura da a volta nas esferas e usa o GL_CLAMP_TO_EDGE para esticar os pixels das bordas
    GLint repetiçao = fixarBorda ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repetiçao);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repetiçao);
    
    //transfere os dados dos pixels da memória ram para a placa de vídeo
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, largura, altura, 0, GL_RGBA, GL_UNSIGNED_BYTE, dados);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA8, largura, altura, GL_RGBA, GL_UNSIGNED_BYTE, dados); //cria os MIPmaps
    
    stbi_image_free(dados); //limpa dados da memória ram
    return idTextura;
}

//Classe planeta
class Planeta {
public:

    float raio;//Tamanho da esfera
    float raioOrbita;//Distância em relação ao Sol
    float velocidadeOrbita;//translação
    float velocidadeRotacao;//Rotação
    float rotacaoAtual;//Ângulo atual da rotação
    GLuint textura;//ID da imagem aplicada a esfera
    bool temAnel;//Define se o planeta tem anel

    //Função que cria o objeto e inicializa seus valores
    Planeta(float r, float rO, float vO, float vR, GLuint tex, bool anel = false) 
        : raio(r), raioOrbita(rO), velocidadeOrbita(vO), velocidadeRotacao(vR), textura(tex), temAnel(anel) {
        rotacaoAtual = 0.0f; //todos os planetas começam com rotação 0
    }

    //Desenha os planetas
    void draw(float t) {
        // Cálculo da posição na órbita usando Trigonometria (Seno e Cosseno)
        float angulo = t * velocidadeOrbita;
        float x = raioOrbita * cos(angulo);
        float z = raioOrbita * sin(angulo);

        //Função para mostrar a orbita
        if (mostrarOrbitas) {
            glDisable(GL_LIGHTING);// Desliga luz para a linha ser constante
            glColor3f(28.0f, 28.0f, 28.0f);// Define cor
            glBegin(GL_LINE_LOOP);// Inicia um loop de linhas conectadas
            for(int i = 0; i < 200; i++) {
                float theta = 2.0f * PI * i / 200.0f; //Divide o círculo em 200 partes
                glVertex3f(raioOrbita * cos(theta), 0, raioOrbita * sin(theta));
            }
            glEnd();
            glEnable(GL_LIGHTING);
        }

        //Transformações dos planetas
        glPushMatrix();// Salva o estado atual da matriz (mundo)
            glTranslatef(x, 0, z);// Move o sistema de coordenadas para a órbita

            //Desenha anel caso o planeta tenha
            if(temAnel) {
                glDisable(GL_LIGHTING);
                glColor3f(0.6f, 0.5f, 0.4f);
                glBegin(GL_QUAD_STRIP);// Desenha uma fita circular preenchida
                for(int i = 0; i <= 60; i++) {
                    float a = 2.0f * PI * i / 60.0f;
                    // Define o raio interno e externo do anel
                    glVertex3f(raio * 1.3f * cos(a), 0, raio * 1.3f * sin(a));
                    glVertex3f(raio * 1.8f * cos(a), 0, raio * 1.8f * sin(a));
                }
                glEnd();
                glEnable(GL_LIGHTING);
            }

            //Rotação
            glRotatef(rotacaoAtual, 0, 1, 0); 
            
            glColor3f(1, 1, 1);// reseta as cores para não interferir na textura
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
            glEnable(GL_TEXTURE_2D);//habilita o uso de texturas
            glBindTexture(GL_TEXTURE_2D, textura); // Seleciona a textura deste planeta
            
            //Desenho da esfera
            GLUquadric* quad = gluNewQuadric();//Cria um objeto quadriculado (auxiliar)
            gluQuadricTexture(quad, GL_TRUE);// Avisa que a esfera deve aceitar textura
            gluSphere(quad, raio, 60, 60);// Desenha a esfera com 60 divisões
            gluDeleteQuadric(quad);// Deleta o auxiliar para liberar memória
            
            glDisable(GL_TEXTURE_2D);// Desabilita textura para o próximo objeto
        glPopMatrix();// Restaura a matriz (volta ao centro do Sol)
    }

    //Atualiza a animação interna do planeta
    void update() { 
        rotacaoAtual += velocidadeRotacao;// Incrementa o ângulo da rotação local
        if(rotacaoAtual > 360){
            rotacaoAtual -= 360;// Mantém o ângulo entre 0 e 360
        }
    }
};

// Lista dinamica que armazena os ponteiros para os planetas criados
std::vector<Planeta*> sistemaSolar;

//ID da textura do sol (desenhado separadamente no centro)
GLuint texturaSol;

//Gera o campo de estrelas
void initializeStars() {
    srand(time(NULL));

    for (int i = 0; i < NUM_ESTRELAS; ++i) {
        //Gera dois valores aleatórios normalizados (entre 0.0 e 1.0)
        float u = (float)rand() / RAND_MAX;
        float v = (float)rand() / RAND_MAX;

        //Faz o calculo de uma distribuição uniforme na esfera
        float theta = 2.0f * PI * u;
        
        // Z define a altura na esfera, variando de -1 a 1
        float z = 2.0f * v - 1.0f;
        
        //R é o raio horizontal no plano XZ para manter o ponto na superfície da esfera
        float r = sqrtf(1.0f - z * z);

        //Posiciona no espaço garatindo que sejam empurrados para longe do centro
        estrelas[i].x = RAIO_ESTRELAS * r * cosf(theta);
        estrelas[i].y = RAIO_ESTRELAS * r * sinf(theta);
        estrelas[i].z = RAIO_ESTRELAS * z;

        //Define uma intensidade de brilho aleatória (entre 0.5 e 1.0)
        estrelas[i].brilho = 0.5f + (float)rand() / RAND_MAX * 0.5f;
        
        //Causa efeito de piscar, pois cada estrela terá um tempo de início diferente para o brilho oscilar
        estrelas[i].fase = (float)rand() / RAND_MAX * 15.0f;
    }
}

void drawStars() {
    // Desativa a iluminação: estrelas possuem brilho próprio (emissivo)
    glDisable(GL_LIGHTING);
    
    // Desativa a escrita no Buffer de Profundidade para evitar que as 
    // estrelas bloqueiem outros objetos e para otimizar a renderização do fundo
    glDepthMask(GL_FALSE);

    // Inicia a renderização de pontos individuais
    glBegin(GL_POINTS);
    for (int i = 0; i < NUM_ESTRELAS; ++i) {
        // Cálculo da cintilação:
        // Usa a função seno para oscilar o brilho
        // A fase evita que todas as estrelas pisquem sincronizadas
        float s = estrelas[i].brilho * (0.5f + 0.3f * sinf(0.6f * tempo + estrelas[i].fase));
        
        // Define a cor (R, G, B) usando o mesmo valor 's' para tons de cinza/branco
        glColor3f(s, s, s);
        
        // Define a posição da estrela no espaço 3D
        glVertex3f(estrelas[i].x, estrelas[i].y, estrelas[i].z);
    }
    glEnd(); // Finaliza o desenho dos pontos

    //Restaura as configurações originais para não afetar o restante da cena
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}

void initlights() {
    // Habilita o sistema global de iluminação do OpenGL
    glEnable(GL_LIGHTING); 
    
    // Habilita especificamente a primeira fonte de luz (Luz 0)
    glEnable(GL_LIGHT0);

    // Define as intensidades da luz:
    // ambiente: luz básica que atinge todos os lados dos objetos (cinza escuro)
    // difusa: luz direcional que define a cor e o brilho principal (branco levemente amarelado/quente)
    GLfloat ambiente[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat difusa[] = {2.0f, 2.0f, 1.8f, 1.0f};

    // Aplica as propriedades de cor ambiente e difusa à Luz 0
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambiente);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, difusa);

    // Habilita o Color Material: permite que a cor definida por glColor*() 
    // seja usada para calcular as propriedades do material do objeto
    glEnable(GL_COLOR_MATERIAL);

    // Configura para que a cor atual do desenho (glColor) afete tanto 
    // a refletividade Ambiente quanto a Difusa da face frontal dos polígonos
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
}


void display() {
    // Limpa os buffers de cor e de profundidade para desenhar o novo quadro
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Desenha o fundo de estrelas
    drawStars();
    // Reseta a matriz de transformação atual (Matriz Modelview)
    glLoadIdentity();

    // Cálculo da posição da câmera em coordenadas polares para efeito de órbita
    float cx = distanciaCamera * cos(anguloCamera + anguloCameraManual);
    float cz = distanciaCamera * sin(anguloCamera + anguloCameraManual);
    
    // Define o ponto de vista: (cx, altura, cz) olhando para a origem (0,0,0)
    // O vetor (0, 1, 0) indica que o eixo Y aponta para cima
    gluLookAt(cx, alturaCamera, cz, 0, 0, 0, 0, 1, 0);

    // Inclina todo o sistema levemente para um visual mais dinâmico
    glRotatef(5.0f, 1.0f, 0.0f, 0.0f);

    
    // Define a posição da Luz 0 no centro da cena (0,0,0) - A posição do Sol
    // O '1' no final indica que é uma luz pontual (posicional)
    GLfloat posicaoLuz[] = {0, 0, 0, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, posicaoLuz);

    //Renderização do sol
    glEnable(GL_TEXTURE_2D);        //Habilita o uso de texturas
    glBindTexture(GL_TEXTURE_2D, texturaSol); //Seleciona a textura do Sol
    
    //Faz o Sol "brilhar" visualmente definindo uma cor de emissão de material
    GLfloat emissao[] = {1.2f, 1.1f, 0.8f, 1.0f};
    glMaterialfv(GL_FRONT, GL_EMISSION, emissao);
    
    //cria uma esfera para representar o Sol
    GLUquadric* s = gluNewQuadric();
    gluQuadricTexture(s, GL_TRUE);// Ativa coordenadas de textura para a esfera
    gluSphere(s, 25.0, 60, 60);// Desenha esfera com raio 10 e 50 subdivisões
    
    //Reseta a emissão para que os próximos objetos (planetas) não brilhem sozinhos
    GLfloat semEmissao[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glMaterialfv(GL_FRONT, GL_EMISSION, semEmissao);
    glDisable(GL_TEXTURE_2D);

    //Renderização dos planetas
    //Percorre o container de planetas e chama o método de desenho de cada um
    for (auto p : sistemaSolar) p->draw(tempo);

    // Troca os buffers (Double Buffering) para exibir o que foi desenhado na tela
    glutSwapBuffers();
}

void updateLogic(int valor) {
    //Só atualiza os valores se a simulação não estiver pausada
    if (!pausado) {
        tempo += 0.04f;//Avança o relógio global do sistema
        anguloCamera += 0.002f;//Cria uma rotação automática da câmera ao redor do Sol
        
        //Percorre todos os objetos do sistema solar e atualiza suas órbitas e rotações
        for (auto p : sistemaSolar) p->update();
    }

    //Solicita ao GLUT que redesenhe o display
    glutPostRedisplay();

    //Reagenda esta mesma função para ser executada novamente daqui a 16 milissegundos
    //Isso mantém o simulador rodando a aproximadamente 60 quadros por segundo (FPS)
    glutTimerFunc(16, updateLogic, 0);
}


void reshape(int l, int a) {
    // Define a área de desenho para ocupar a janela inteira
    glViewport(0, 0, l, a);
    
    // Muda para a matriz de projeção para configurar a lente da câmera
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    
    // Configura a perspectiva 3D:
    // 60.0: Campo de visão (FOV) em graus
    // (float)l/a: Proporção da tela (aspect ratio)
    // 0.5: Plano de corte próximo (near clipping plane)
    // 350.0: Plano de corte distante (far clipping plane)
    gluPerspective(60.0, (float)l/a, 0.5, 1500.0);
    
    // Volta para a matriz de modelo para começar a desenhar os objetos
    glMatrixMode(GL_MODELVIEW);
}

void initializeConfig() {
    glEnable(GL_DEPTH_TEST);   // Habilita o teste de profundidade (Z-buffer)
    glEnable(GL_NORMALIZE);    // Normaliza vetores normais
    
    // Configura a textura para misturar com a iluminação (não apenas sobrepor)
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    
    // Chama as funções de setup que definimos anteriormente
    initializeStars();
    initlights();

    // Carrega a textura do Sol
    texturaSol = loadTexture("texturas/8k_sun.jpg", true);
    
    //Adiciona os planetas ao vetor 'sistemaSolar'
    //Parâmetros: (raio, distância_do_sol, velocidade_orbita, velocidade_rotacao, textura, temAnéis)
    //https://science-nasa-gov.translate.goog/solar-system/planets/planet-sizes-and-locations-in-our-solar-system/?_x_tr_sl=en&_x_tr_tl=pt&_x_tr_hl=pt&_x_tr_pto=tc
    //https://www.solarsystemscope.com/textures/
    sistemaSolar.push_back(new Planeta(2.0f,  50.0f,  0.60f, 2.0f, loadTexture("texturas/8k_mercury.jpg")));
    sistemaSolar.push_back(new Planeta(3.8f,  75.0f,  0.55f, 1.8f, loadTexture("texturas/4k_venus_atmosphere.jpg")));
    sistemaSolar.push_back(new Planeta(4.0f,  100.0f, 0.50f, 1.6f, loadTexture("texturas/8k_earth_daymap.jpg")));
    sistemaSolar.push_back(new Planeta(2.5f,  125.0f, 0.45f, 1.5f, loadTexture("texturas/8k_mars.jpg")));
    sistemaSolar.push_back(new Planeta(14.0f, 180.0f, 0.30f, 1.2f, loadTexture("texturas/8k_jupiter.jpg")));
    sistemaSolar.push_back(new Planeta(12.0f, 240.0f, 0.25f, 1.1f, loadTexture("texturas/8k_saturn.jpg"), true));
    sistemaSolar.push_back(new Planeta(8.0f,  300.0f, 0.20f, 1.0f, loadTexture("texturas/2k_uranus.jpg")));
    sistemaSolar.push_back(new Planeta(7.8f,  360.0f, 0.15f, 0.9f, loadTexture("texturas/2k_neptune.jpg")));
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        // Zoom: 'z' para afastar e 'Z' para aproximar
        case 'z': 
            distanciaCamera += 5.0f; 
            break;
        case 'Z': 
            distanciaCamera -= 5.0f; 
            break;

        // Altura: 'w' para subir e 's' para descer
        case 'w': 
            alturaCamera += 2.0f; 
            break;
        case 's': 
            alturaCamera -= 2.0f; 
            break;

        // Rotação Manual
        case 'a': 
            anguloCameraManual += 0.05f; 
            break;
        case 'd': 
            anguloCameraManual -= 0.05f; 
            break;

        // Controle da órbita
        case 'p': 
            pausado = !pausado; 
            break;
        case 'o': 
            mostrarOrbitas = !mostrarOrbitas; 
            break;
        case 27: 
            exit(0); 
            break;
    }

    glutPostRedisplay();
}

int main(int argc, char** argv) {
    // Inicializa o ambiente GLUT
    glutInit(&argc, argv);
    
    // Configura o modo de exibição:
    // GLUT_DOUBLE: Buffer duplo para evitar cintilação (flicker)
    // GLUT_RGB: Sistema de cores Vermelho, Verde e Azul
    // GLUT_DEPTH: Buffer de profundidade para 3D real
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    
    glutInitWindowSize(1000, 800);
    glutCreateWindow("Sistema Solar");
    
    // Executa as nossas configurações de cena e objetos
    initializeConfig();
    
    // Registra as funções de retorno (Callbacks)
    glutDisplayFunc(display);//unção de desenho
    glutReshapeFunc(reshape);//Função de ajuste de janela
    glutKeyboardFunc(keyboard);// Função de comandos de teclado
    glutTimerFunc(0, updateLogic, 0);// Inicia o loop de animação
    
    // Entra no loop infinito do OpenGL, aguardando eventos
    glutMainLoop();
    return 0;
}