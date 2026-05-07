#include <iostream>
#include <BRepPrimAPI_MakeBox.hxx>
#include <STEPControl_Writer.hxx>
#include <Interface_Static.hxx>

int main() {
    try {
        TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0);
        
        STEPControl_Writer writer;
        Interface_Static::SetCVal("write.step.schema", "AP203");
        writer.Transfer(box, STEPControl_AsIs);
        
        std::string outputPath = "C:\\Users\\bangmin.wu\\Desktop\\TEST_CONVERT2\\inputfile\\test_box.step";
        IFSelect_ReturnStatus status = writer.Write(outputPath.c_str());
        
        if (status == IFSelect_RetDone) {
            std::cout << "Test STEP file created successfully!" << std::endl;
            return 0;
        } else {
            std::cout << "Failed to write STEP file" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
}